#include <auralis/audio/NativeAudioManager.h>

#include <auralis/audio/AudioEndpointListModel.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/IPipeWireLinkBackend.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/audio/PipeWireTypes.h>
#include <auralis/bluetooth/BlueZTypes.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/LoggingCategories.h>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QHash>
#include <QIODevice>
#include <QMediaDevices>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace auralis::audio {
namespace {

QString nativeDeviceToken(const QAudioDevice& device)
{
    return QString::fromLatin1(device.id().toHex());
}

quint64 nativeDeviceSerial(const QAudioDevice& device, const QByteArray& role)
{
    const QByteArray digest = QCryptographicHash::hash(device.id() + ':' + role, QCryptographicHash::Sha256);
    quint64 serial = 0;
    for (int index = 0; index < 8; ++index) {
        serial = (serial << 8U) | static_cast<quint8>(digest.at(index));
    }
    return serial == 0 ? 1 : serial;
}

QString normalizedName(QString value)
{
    value = value.toLower().simplified();
    value.remove(QLatin1Char('-'));
    value.remove(QLatin1Char('_'));
    return value;
}

void sortNativeDevices(QList<QAudioDevice>& devices, const QAudioDevice& defaultDevice)
{
    const QByteArray defaultId = defaultDevice.id();
    std::sort(devices.begin(), devices.end(), [&defaultId](const QAudioDevice& left, const QAudioDevice& right) {
        const bool leftIsDefault = !defaultId.isEmpty() && left.id() == defaultId;
        const bool rightIsDefault = !defaultId.isEmpty() && right.id() == defaultId;
        if (leftIsDefault != rightIsDefault) {
            return leftIsDefault;
        }
        return left.id() < right.id();
    });
}

QByteArray nativeGraphFingerprint(
    const QList<QAudioDevice>& inputs,
    const QList<QAudioDevice>& outputs,
    const bluetooth::DeviceRegistry* bluetoothRegistry)
{
    QByteArray serialized;
    QDataStream stream(&serialized, QIODevice::WriteOnly);
    stream << QMediaDevices::defaultAudioInput().id()
           << QMediaDevices::defaultAudioOutput().id();

    const auto appendAudioDevices = [&stream](const QList<QAudioDevice>& devices, const QByteArray& role) {
        stream << role << static_cast<qint32>(devices.size());
        for (const QAudioDevice& device : devices) {
            const QAudioFormat preferred = device.preferredFormat();
            stream << device.id()
                   << device.description()
                   << preferred.sampleRate()
                   << preferred.channelCount()
                   << static_cast<qint32>(preferred.sampleFormat());
        }
    };
    appendAudioDevices(inputs, QByteArrayLiteral("inputs"));
    appendAudioDevices(outputs, QByteArrayLiteral("outputs"));

    QVector<bluetooth::BluetoothDeviceData> bluetoothDevices = bluetoothRegistry != nullptr
        ? bluetoothRegistry->devices()
        : QVector<bluetooth::BluetoothDeviceData>{};
    std::sort(
        bluetoothDevices.begin(),
        bluetoothDevices.end(),
        [](const auto& left, const auto& right) { return left.objectPath < right.objectPath; });
    stream << static_cast<qint32>(bluetoothDevices.size());
    for (const bluetooth::BluetoothDeviceData& device : bluetoothDevices) {
        // Only fields used by endpoint correlation belong here. Volatile scan
        // metadata (RSSI, lastSeen, reconnect attempts, operations) must not
        // tear down and rebuild active audio routes.
        stream << device.objectPath << device.address << device.displayName();
    }

    return QCryptographicHash::hash(serialized, QCryptographicHash::Sha256);
}

PipeWireObjectSnapshot nodeSnapshot(
    quint32 id,
    quint64 serial,
    const QAudioDevice& device,
    const QString& mediaClass,
    const QString& api,
    bool isDefault)
{
    const QAudioFormat format = device.preferredFormat();
    PipeWireObjectSnapshot snapshot;
    snapshot.globalId = id;
    snapshot.interfaceType = QStringLiteral("PipeWire:Interface:Node");
    snapshot.kind = PipeWireObjectKind::Node;
    snapshot.properties = PipeWireProperties::fromHash({
        {QStringLiteral("object.serial"), QString::number(serial)},
        {QStringLiteral("node.name"), QStringLiteral("native.%1").arg(nativeDeviceToken(device))},
        {QStringLiteral("node.description"), device.description()},
        {QStringLiteral("media.class"), mediaClass},
        {QStringLiteral("device.api"), api},
        {QStringLiteral("device.default"), isDefault ? QStringLiteral("true") : QStringLiteral("false")},
        {QStringLiteral("audio.rate"), QString::number(format.sampleRate())},
        {QStringLiteral("audio.channels"), QString::number(format.channelCount())},
    });
    return snapshot;
}

PipeWireObjectSnapshot portSnapshot(quint32 id, quint32 nodeId, bool output)
{
    PipeWireObjectSnapshot snapshot;
    snapshot.globalId = id;
    snapshot.interfaceType = QStringLiteral("PipeWire:Interface:Port");
    snapshot.kind = PipeWireObjectKind::Port;
    snapshot.properties = PipeWireProperties::fromHash({
        {QStringLiteral("object.serial"), QString::number(id)},
        {QStringLiteral("node.id"), QString::number(nodeId)},
        {QStringLiteral("port.name"), output ? QStringLiteral("capture") : QStringLiteral("playback")},
        {QStringLiteral("port.direction"), output ? QStringLiteral("out") : QStringLiteral("in")},
        {QStringLiteral("port.physical"), QStringLiteral("true")},
        {QStringLiteral("port.terminal"), QStringLiteral("true")},
    });
    return snapshot;
}

class NativeAudioLinkBackend final : public QObject, public IPipeWireLinkBackend {
public:
    explicit NativeAudioLinkBackend(PipeWireObjectStore* store, QObject* parent = nullptr)
        : QObject(parent)
        , store_(store)
    {
    }

    void setErrorHandler(std::function<void(const QString&)> handler)
    {
        errorHandler_ = std::move(handler);
    }

    void registerInput(quint32 nodeId, const QAudioDevice& device)
    {
        inputs_.insert(nodeId, device);
    }

    void registerOutput(quint32 nodeId, const QAudioDevice& device)
    {
        outputs_.insert(nodeId, device);
    }

    void clearDevices()
    {
        clearLinks();
        inputs_.clear();
        outputs_.clear();
        volumes_.clear();
        muted_.clear();
    }

    std::optional<LinkCreateResult> createLink(
        quint32 outputNode,
        quint32 outputPort,
        quint32 inputNode,
        quint32 inputPort,
        const QString& routeId,
        const QHash<QString, QString>& extraProps) override
    {
        if (!inputs_.contains(outputNode) || !outputs_.contains(inputNode)) {
            reportError(QStringLiteral("The selected native audio devices are no longer available."));
            return std::nullopt;
        }

        const quint64 token = nextToken_++;
        const quint32 globalId = nextGlobalId_++;
        Link link{globalId, outputNode, outputPort, inputNode, inputPort, routeId};
        links_.insert(token, link);
        if (!restartStreams()) {
            links_.remove(token);
            restartStreams();
            return std::nullopt;
        }

        QHash<QString, QString> properties = extraProps;
        properties.insert(QStringLiteral("link.output.node"), QString::number(outputNode));
        properties.insert(QStringLiteral("link.output.port"), QString::number(outputPort));
        properties.insert(QStringLiteral("link.input.node"), QString::number(inputNode));
        properties.insert(QStringLiteral("link.input.port"), QString::number(inputPort));
        properties.insert(QStringLiteral("link.state"), QStringLiteral("active"));
        properties.insert(QStringLiteral("auralis.route.id"), routeId);
        PipeWireObjectSnapshot snapshot;
        snapshot.globalId = globalId;
        snapshot.interfaceType = QStringLiteral("PipeWire:Interface:Link");
        snapshot.kind = PipeWireObjectKind::Link;
        snapshot.properties = PipeWireProperties::fromHash(std::move(properties));
        store_->upsert(snapshot);
        return LinkCreateResult{token, globalId};
    }

    bool destroyOwnedLink(quint64 token) override
    {
        const auto it = links_.find(token);
        if (it == links_.end()) {
            return false;
        }
        store_->remove(it->globalId);
        links_.erase(it);
        restartStreams();
        return true;
    }

    bool destroyForeignLink(quint32 globalId) override
    {
        for (auto it = links_.begin(); it != links_.end(); ++it) {
            if (it->globalId == globalId) {
                return destroyOwnedLink(it.key());
            }
        }
        return store_->remove(globalId);
    }

    quint32 ownedLinkGlobalId(quint64 token) const override
    {
        return links_.value(token).globalId;
    }

    bool setNodeVolume(quint32 nodeId, double volume) override
    {
        if (!outputs_.contains(nodeId)) {
            return false;
        }
        volumes_.insert(nodeId, std::clamp(volume, 0.0, 1.0));
        applyLevels();
        return true;
    }

    bool setNodeMuted(quint32 nodeId, bool muted) override
    {
        if (!outputs_.contains(nodeId)) {
            return false;
        }
        if (muted) {
            muted_.insert(nodeId);
        } else {
            muted_.remove(nodeId);
        }
        applyLevels();
        return true;
    }

    bool volumeSupported(quint32 nodeId) const override
    {
        return outputs_.contains(nodeId);
    }

private:
    struct Link {
        quint32 globalId = 0;
        quint32 outputNode = 0;
        quint32 outputPort = 0;
        quint32 inputNode = 0;
        quint32 inputPort = 0;
        QString routeId;
    };

    struct RenderTarget {
        quint32 nodeId = 0;
        std::unique_ptr<QAudioSink> sink;
        QIODevice* device = nullptr;
    };

    std::optional<QAudioFormat> commonFormat(
        const QAudioDevice& input,
        const QSet<quint32>& renderNodes) const
    {
        QVector<QAudioFormat> candidates;
        candidates.push_back(input.preferredFormat());
        for (quint32 nodeId : renderNodes) {
            candidates.push_back(outputs_.value(nodeId).preferredFormat());
        }
        const QList<int> rates{48000, 44100, input.preferredFormat().sampleRate()};
        const QList<int> channels{2, 1, input.preferredFormat().channelCount()};
        const QList<QAudioFormat::SampleFormat> samples{
            QAudioFormat::Float,
            QAudioFormat::Int16,
            QAudioFormat::Int32,
            QAudioFormat::UInt8,
        };
        for (int rate : rates) {
            for (int channelCount : channels) {
                for (QAudioFormat::SampleFormat sample : samples) {
                    QAudioFormat candidate;
                    candidate.setSampleRate(rate);
                    candidate.setChannelCount(channelCount);
                    candidate.setSampleFormat(sample);
                    candidates.push_back(candidate);
                }
            }
        }
        for (const QAudioFormat& candidate : candidates) {
            if (!candidate.isValid() || !input.isFormatSupported(candidate)) {
                continue;
            }
            bool supported = true;
            for (quint32 nodeId : renderNodes) {
                if (!outputs_.value(nodeId).isFormatSupported(candidate)) {
                    supported = false;
                    break;
                }
            }
            if (supported) {
                return candidate;
            }
        }
        return std::nullopt;
    }

    void stopStreams()
    {
        if (source_ != nullptr) {
            source_->stop();
        }
        for (RenderTarget& target : renders_) {
            if (target.sink != nullptr) {
                target.sink->stop();
            }
        }
        captureDevice_ = nullptr;
        source_.reset();
        renders_.clear();
    }

    void clearLinks()
    {
        stopStreams();
        for (const Link& link : links_) {
            store_->remove(link.globalId);
        }
        links_.clear();
    }

    bool restartStreams()
    {
        stopStreams();
        if (links_.isEmpty()) {
            return true;
        }

        quint32 captureNode = 0;
        QSet<quint32> renderNodes;
        for (const Link& link : links_) {
            if (captureNode != 0 && captureNode != link.outputNode) {
                reportError(QStringLiteral("Only one capture source can be active in a native route."));
                return false;
            }
            captureNode = link.outputNode;
            renderNodes.insert(link.inputNode);
        }

        const QAudioDevice input = inputs_.value(captureNode);
        if (input.isNull()) {
            reportError(QStringLiteral("The selected native capture source has no usable audio format."));
            return false;
        }
        const std::optional<QAudioFormat> selectedFormat = commonFormat(input, renderNodes);
        if (!selectedFormat.has_value()) {
            reportError(QStringLiteral("The selected devices do not share a native PCM format."));
            return false;
        }
        const QAudioFormat format = *selectedFormat;

        for (quint32 nodeId : renderNodes) {
            RenderTarget target;
            target.nodeId = nodeId;
            target.sink = std::make_unique<QAudioSink>(outputs_.value(nodeId), format, this);
            target.device = target.sink->start();
            if (target.device == nullptr) {
                reportError(QStringLiteral("Unable to start native audio output %1.").arg(outputs_.value(nodeId).description()));
                stopStreams();
                return false;
            }
            renders_.push_back(std::move(target));
        }
        applyLevels();

        source_ = std::make_unique<QAudioSource>(input, format, this);
        captureDevice_ = source_->start();
        if (captureDevice_ == nullptr) {
            reportError(QStringLiteral("Unable to start native audio capture from %1.").arg(input.description()));
            stopStreams();
            return false;
        }
        connect(captureDevice_, &QIODevice::readyRead, this, [this, format]() {
            QByteArray pcm = captureDevice_ != nullptr ? captureDevice_->readAll() : QByteArray();
            if (pcm.isEmpty()) {
                return;
            }
            const qsizetype frameBytes = std::max(1, format.bytesPerFrame());
            for (RenderTarget& target : renders_) {
                if (target.device == nullptr || target.sink == nullptr) {
                    continue;
                }
                const qsizetype writable = std::max<qsizetype>(0, target.sink->bytesFree());
                const qsizetype aligned = std::min(pcm.size(), writable) / frameBytes * frameBytes;
                if (aligned > 0) {
                    target.device->write(pcm.constData(), static_cast<qint64>(aligned));
                }
            }
        });
        return true;
    }

    void applyLevels()
    {
        for (RenderTarget& target : renders_) {
            const double volume = muted_.contains(target.nodeId) ? 0.0 : volumes_.value(target.nodeId, 1.0);
            target.sink->setVolume(static_cast<qreal>(volume));
        }
    }

    void reportError(const QString& error)
    {
        if (errorHandler_) {
            errorHandler_(error);
        }
    }

    PipeWireObjectStore* store_ = nullptr;
    QHash<quint32, QAudioDevice> inputs_;
    QHash<quint32, QAudioDevice> outputs_;
    QHash<quint64, Link> links_;
    QHash<quint32, double> volumes_;
    QSet<quint32> muted_;
    std::unique_ptr<QAudioSource> source_;
    QIODevice* captureDevice_ = nullptr;
    std::vector<RenderTarget> renders_;
    std::function<void(const QString&)> errorHandler_;
    quint64 nextToken_ = 1;
    quint32 nextGlobalId_ = 0x70000000U;
};

QString nativeAudioApiName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("WASAPI/Qt Multimedia");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("Core Audio/Qt Multimedia");
#else
    return QStringLiteral("Qt Multimedia");
#endif
}

} // namespace

struct NativeAudioManager::State {
    QMediaDevices mediaDevices;
    std::unique_ptr<PipeWireObjectStore> store = std::make_unique<PipeWireObjectStore>();
    NativeAudioLinkBackend* backend = nullptr;
    QTimer graphRefreshTimer;
    QByteArray graphFingerprint;
};

NativeAudioManager::NativeAudioManager(bluetooth::DeviceRegistry* bluetoothRegistry, QObject* parent)
    : QObject(parent)
    , stateImpl_(std::make_unique<State>())
    , bluetoothRegistry_(bluetoothRegistry)
    , endpoints_(new AudioEndpointRegistry(this))
    , model_(new AudioEndpointListModel(endpoints_, this))
{
    stateImpl_->backend = new NativeAudioLinkBackend(stateImpl_->store.get(), this);
    stateImpl_->backend->setErrorHandler([this](const QString& error) { setLastError(error); });
    router_ = new AudioRouter(stateImpl_->store.get(), endpoints_, stateImpl_->backend, this);
    stateImpl_->graphRefreshTimer.setSingleShot(true);
    stateImpl_->graphRefreshTimer.setInterval(300);
    connect(&stateImpl_->graphRefreshTimer, &QTimer::timeout, this, &NativeAudioManager::refreshGraph);
    connect(&stateImpl_->mediaDevices, &QMediaDevices::audioInputsChanged, this, &NativeAudioManager::scheduleGraphRefresh);
    connect(&stateImpl_->mediaDevices, &QMediaDevices::audioOutputsChanged, this, &NativeAudioManager::scheduleGraphRefresh);
    if (bluetoothRegistry_ != nullptr) {
        connect(bluetoothRegistry_, &bluetooth::DeviceRegistry::deviceAdded, this, &NativeAudioManager::scheduleGraphRefresh);
        connect(bluetoothRegistry_, &bluetooth::DeviceRegistry::deviceUpdated, this, &NativeAudioManager::scheduleGraphRefresh);
        connect(bluetoothRegistry_, &bluetooth::DeviceRegistry::deviceRemoved, this, &NativeAudioManager::scheduleGraphRefresh);
    }
}

NativeAudioManager::~NativeAudioManager()
{
    shutdown();
}

bool NativeAudioManager::initialize()
{
    if (status_ == core::ServiceStatus::Ready) {
        return true;
    }
    status_ = core::ServiceStatus::Initializing;
    emit statusChanged();
    refreshGraph();
    status_ = core::ServiceStatus::Ready;
    emit statusChanged();
    emit connectionStateChanged();
    return true;
}

void NativeAudioManager::shutdown()
{
    if (status_ == core::ServiceStatus::Uninitialized) {
        return;
    }
    router_->handleConnectionState(PipeWireConnectionState::Stopping, false);
    router_->shutdown();
    stateImpl_->graphRefreshTimer.stop();
    stateImpl_->graphFingerprint.clear();
    stateImpl_->backend->clearDevices();
    endpoints_->clear();
    stateImpl_->store->clear();
    status_ = core::ServiceStatus::Uninitialized;
    emit statusChanged();
    emit connectionStateChanged();
}

core::ServiceStatus NativeAudioManager::status() const noexcept { return status_; }
QObject* NativeAudioManager::uiObject() { return this; }
QString NativeAudioManager::backendName() const { return nativeAudioApiName(); }
QString NativeAudioManager::lastError() const { return lastError_; }
bool NativeAudioManager::connected() const noexcept { return status_ == core::ServiceStatus::Ready; }
bool NativeAudioManager::graphReady() const noexcept { return connected(); }
void NativeAudioManager::setAutoReconnectEnabled(bool enabled) { autoReconnectEnabled_ = enabled; }
void NativeAudioManager::requestReconnect()
{
    emit reconnectAttemptStarted(1);
    refreshGraph();
}
AudioRouter* NativeAudioManager::audioRouter() const noexcept { return router_; }
AudioEndpointRegistry* NativeAudioManager::endpointRegistry() const noexcept { return endpoints_; }
QString NativeAudioManager::connectionStateText() const { return connected() ? QStringLiteral("Connected") : QStringLiteral("Stopped"); }
int NativeAudioManager::endpointCount() const { return endpoints_->count(); }
int NativeAudioManager::deviceCount() const { return endpointCount(); }
int NativeAudioManager::nodeCount() const { return stateImpl_->store->nodeCount(); }
int NativeAudioManager::mappedBluetoothCount() const { return static_cast<int>(endpoints_->mappedBluetoothEndpoints().size()); }
bool NativeAudioManager::initialSyncComplete() const noexcept { return connected(); }
int NativeAudioManager::graphRevision() const noexcept { return graphRevision_; }
QString NativeAudioManager::diagnosticsText() const
{
    return QStringLiteral("backend=%1 state=%2 inputs=%3 outputs=%4 mappedBt=%5 error=%6")
        .arg(
            backendName(),
            connectionStateText(),
            QString::number(QMediaDevices::audioInputs().size()),
            QString::number(QMediaDevices::audioOutputs().size()),
            QString::number(mappedBluetoothCount()),
            lastError_.isEmpty() ? QStringLiteral("none") : lastError_);
}
QAbstractItemModel* NativeAudioManager::endpoints() const { return model_; }
QObject* NativeAudioManager::router() const { return router_; }

QString NativeAudioManager::audioStatusForDevice(const QString& bluetoothDeviceId) const
{
    if (bluetoothDeviceId.isEmpty()) {
        return {};
    }
    if (!endpoints_->endpointsForBluetoothDevice(bluetoothDeviceId).isEmpty()) {
        return QStringLiteral("Available");
    }
    const auto* device = bluetoothRegistry_ != nullptr
        ? bluetoothRegistry_->findByObjectPath(bluetoothDeviceId)
        : nullptr;
    return device != nullptr && device->connected ? QStringLiteral("Initializing...") : QStringLiteral("Unavailable");
}

void NativeAudioManager::scheduleGraphRefresh()
{
    if (status_ != core::ServiceStatus::Uninitialized) {
        stateImpl_->graphRefreshTimer.start();
    }
}

void NativeAudioManager::refreshGraph()
{
    if (status_ == core::ServiceStatus::Uninitialized) {
        return;
    }
    setLastError({});

    QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
    QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    sortNativeDevices(inputs, QMediaDevices::defaultAudioInput());
    sortNativeDevices(outputs, QMediaDevices::defaultAudioOutput());

    const QByteArray fingerprint = nativeGraphFingerprint(inputs, outputs, bluetoothRegistry_);
    if (fingerprint == stateImpl_->graphFingerprint) {
        return;
    }
    stateImpl_->graphFingerprint = fingerprint;

    router_->handleConnectionState(PipeWireConnectionState::Stopping, false);
    stateImpl_->backend->clearDevices();
    endpoints_->clear();
    stateImpl_->store->clear();

    quint32 nextId = 1;
    for (const QAudioDevice& input : inputs) {
        const quint32 nodeId = nextId++;
        const quint32 portId = nextId++;
        stateImpl_->store->upsert(nodeSnapshot(
            nodeId,
            nativeDeviceSerial(input, QByteArrayLiteral("capture")),
            input,
            QStringLiteral("Audio/Source"),
            backendName(),
            input.id() == QMediaDevices::defaultAudioInput().id()));
        stateImpl_->store->upsert(portSnapshot(portId, nodeId, true));
        stateImpl_->backend->registerInput(nodeId, input);
    }

    for (const QAudioDevice& output : outputs) {
        const quint32 nodeId = nextId++;
        const quint32 portId = nextId++;
        stateImpl_->store->upsert(nodeSnapshot(
            nodeId,
            nativeDeviceSerial(output, QByteArrayLiteral("playback")),
            output,
            QStringLiteral("Audio/Sink"),
            backendName(),
            output.id() == QMediaDevices::defaultAudioOutput().id()));
        stateImpl_->store->upsert(portSnapshot(portId, nodeId, false));
        stateImpl_->backend->registerOutput(nodeId, output);

        AudioEndpoint endpoint;
        endpoint.id = QStringLiteral("native:%1:playback").arg(nativeDeviceToken(output));
        endpoint.pipeWireObjectId = nodeId;
        endpoint.pipeWireSerial = nativeDeviceSerial(output, QByteArrayLiteral("playback"));
        endpoint.name = output.description();
        endpoint.description = output.description();
        endpoint.direction = AudioEndpointDirection::Playback;
        endpoint.mediaClass = QStringLiteral("Audio/Sink");
        endpoint.nodeName = QStringLiteral("native.%1").arg(nativeDeviceToken(output));
        endpoint.sampleRate = static_cast<quint32>(output.preferredFormat().sampleRate());
        endpoint.channelCount = static_cast<quint32>(output.preferredFormat().channelCount());
        endpoint.transport = AudioTransport::BuiltIn;

        QVector<bluetooth::BluetoothDeviceData> matches;
        if (bluetoothRegistry_ != nullptr) {
            const QString outputName = normalizedName(output.description());
            for (const bluetooth::BluetoothDeviceData& device : bluetoothRegistry_->devices()) {
                const QString deviceName = normalizedName(device.displayName());
                if (!deviceName.isEmpty() && (outputName.contains(deviceName) || deviceName.contains(outputName))) {
                    matches.push_back(device);
                }
            }
        }
        if (matches.size() == 1) {
            const auto& match = matches.front();
            endpoint.transport = AudioTransport::BluetoothClassic;
            endpoint.bluetoothDeviceId = match.objectPath;
            endpoint.bluetoothAddress = bluetooth::normalizeBluetoothAddress(match.address)
                                            .value_or(match.address.trimmed().toUpper());
            endpoint.bluetoothDisplayName = match.displayName();
            endpoint.mappingConfidence = EndpointMappingConfidence::Weak;
            endpoint.mappingReason = EndpointMappingReason::UniqueNameFallback;
        }
        endpoints_->upsert(endpoint);
    }

    router_->refreshSources();
    router_->handleConnectionState(PipeWireConnectionState::Connected, true);
    router_->handleGraphChanged();
    ++graphRevision_;
    emit graphRevisionChanged();
    qCInfo(auralisAudio) << "Native audio graph refreshed backend=" << backendName()
                         << "inputs=" << inputs.size() << "outputs=" << outputs.size();
}

void NativeAudioManager::setLastError(const QString& error)
{
    if (lastError_ == error) {
        return;
    }
    lastError_ = error;
    emit lastErrorChanged();
}

} // namespace auralis::audio

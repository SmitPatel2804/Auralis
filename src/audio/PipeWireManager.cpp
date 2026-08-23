#include <auralis/audio/PipeWireManager.h>

#include <auralis/audio/AudioEndpointListModel.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/EndpointResolver.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/LoggingCategories.h>

#include <QCoreApplication>
#include <QProcess>

#include <algorithm>

namespace auralis::audio {

PipeWireManager::PipeWireManager(QObject* parent)
    : PipeWireManager(nullptr, parent)
{
}

PipeWireManager::PipeWireManager(bluetooth::DeviceRegistry* bluetoothRegistry, QObject* parent)
    : QObject(parent)
    , bluetoothRegistry_(bluetoothRegistry)
    , store_(std::make_unique<PipeWireObjectStore>())
    , endpoints_(std::make_unique<AudioEndpointRegistry>())
    , resolver_(std::make_unique<EndpointResolver>(bluetoothRegistry_))
    , connection_(std::make_unique<PipeWireConnection>())
    , model_(new AudioEndpointListModel(endpoints_.get(), this))
    , guard_(std::make_shared<Guard>())
{
    router_ = new AudioRouter(store_.get(), endpoints_.get(), connection_.get(), this);
    connection_->setLinkErrorHandler([this](quint64 token, int res, const QString& message) {
        Q_UNUSED(res)
        QMetaObject::invokeMethod(
            router_,
            [this, token, message]() { router_->handleOwnedLinkError(token, message); },
            Qt::QueuedConnection);
    });
    wireBluetoothRegistry();

    graphRefreshTimer_.setSingleShot(true);
    // PipeWire can deliver a burst for every node/port/link and BlueZ can
    // update RSSI several times per second. Rebuild the public graph at most
    // four times per second instead of once per raw event.
    graphRefreshTimer_.setInterval(250);
    graphRefreshTimer_.setTimerType(Qt::CoarseTimer);
    connect(&graphRefreshTimer_, &QTimer::timeout, this, &PipeWireManager::refreshGraph);

    reconnectTimer_.setSingleShot(true);
    connect(&reconnectTimer_, &QTimer::timeout, this, &PipeWireManager::performReconnect);

    initialSyncTimeoutTimer_.setSingleShot(true);
    connect(&initialSyncTimeoutTimer_, &QTimer::timeout, this, &PipeWireManager::onInitialSyncTimeout);

    virtualOutputTimer_.setSingleShot(true);
    connect(&virtualOutputTimer_, &QTimer::timeout, this, &PipeWireManager::ensureVirtualOutput);
}

PipeWireManager::~PipeWireManager()
{
    shutdown();
}

void PipeWireManager::wireBluetoothRegistry()
{
    if (bluetoothWired_ || bluetoothRegistry_ == nullptr) {
        return;
    }
    bluetoothWired_ = true;
    const auto refresh = [this]() { scheduleGraphRefresh(); };
    connect(bluetoothRegistry_, &bluetooth::DeviceRegistry::deviceAdded, this, refresh);
    connect(bluetoothRegistry_, &bluetooth::DeviceRegistry::deviceUpdated, this, refresh);
    connect(bluetoothRegistry_, &bluetooth::DeviceRegistry::deviceRemoved, this, refresh);
}

QObject* PipeWireManager::uiObject()
{
    return this;
}

QString PipeWireManager::backendName() const
{
    return QStringLiteral("PipeWire");
}

auralis::core::ServiceStatus PipeWireManager::status() const noexcept
{
    return status_;
}

PipeWireConnectionState PipeWireManager::connectionState() const noexcept
{
    return connectionState_;
}

QString PipeWireManager::connectionStateText() const
{
    return toString(connectionState_);
}

bool PipeWireManager::connected() const noexcept
{
    return connectionState_ == PipeWireConnectionState::Connected;
}

bool PipeWireManager::graphReady() const noexcept
{
    return connected() && initialSyncComplete();
}

QString PipeWireManager::lastError() const
{
    return lastError_;
}

int PipeWireManager::endpointCount() const
{
    return endpoints_ != nullptr ? endpoints_->count() : 0;
}

int PipeWireManager::deviceCount() const
{
    return store_ != nullptr ? store_->deviceCount() : 0;
}

int PipeWireManager::nodeCount() const
{
    return store_ != nullptr ? store_->nodeCount() : 0;
}

int PipeWireManager::mappedBluetoothCount() const
{
    return endpoints_ != nullptr ? static_cast<int>(endpoints_->mappedBluetoothEndpoints().size()) : 0;
}

bool PipeWireManager::initialSyncComplete() const noexcept
{
    return initialSyncComplete_;
}

int PipeWireManager::graphRevision() const noexcept
{
    return graphRevision_;
}

QString PipeWireManager::diagnosticsText() const
{
    return QStringLiteral(
               "state=%1 devices=%2 nodes=%3 ports=%4 links=%5 endpoints=%6 mappedBt=%7 unmappedBt=%8 "
               "virtualOutput=%9 defaultSink=%10 error=%11")
        .arg(toString(connectionState_))
        .arg(store_->deviceCount())
        .arg(store_->nodeCount())
        .arg(store_->portCount())
        .arg(store_->linkCount())
        .arg(endpoints_->count())
        .arg(endpoints_->mappedBluetoothEndpoints().size())
        .arg(endpoints_->unmappedBluetoothEndpoints().size())
        .arg(virtualOutputStatus())
        .arg(defaultAudioSinkName_.isEmpty() ? QStringLiteral("unknown") : defaultAudioSinkName_)
        .arg(lastError_.isEmpty() ? QStringLiteral("none") : lastError_);
}

bool PipeWireManager::virtualOutputAvailable() const noexcept
{
    return virtualOutput_.ready();
}

bool PipeWireManager::virtualOutputSelected() const noexcept
{
    return virtualOutput_.ready() && virtualOutput_.selectedAsDefault;
}

QString PipeWireManager::virtualOutputStatus() const
{
    if (virtualOutput_.ready()) {
        if (virtualOutput_.selectedAsDefault) {
            return QStringLiteral("Selected as system output");
        }
        if (virtualOutput_.packageManaged) {
            return QStringLiteral("Ready (persistent)");
        }
        if (virtualOutput_.runtimeManaged) {
            return QStringLiteral("Ready (while Auralis runs)");
        }
        return QStringLiteral("Ready");
    }
    if (!virtualOutputError_.isEmpty()) {
        return QStringLiteral("Unavailable - %1").arg(virtualOutputError_);
    }
    if (virtualOutput_.partial() || virtualOutputProvisionAttempt_ > 0) {
        return QStringLiteral("Starting...");
    }
    return connected() ? QStringLiteral("Waiting for PipeWire") : QStringLiteral("PipeWire unavailable");
}

QAbstractItemModel* PipeWireManager::endpoints() const
{
    return model_;
}

QObject* PipeWireManager::router() const
{
    return router_;
}

AudioRouter* PipeWireManager::audioRouter() const noexcept
{
    return router_;
}

AudioEndpointRegistry* PipeWireManager::endpointRegistry() const noexcept
{
    return endpoints_.get();
}

const PipeWireObjectStore* PipeWireManager::objectStore() const noexcept
{
    return store_.get();
}

QString PipeWireManager::audioStatusForDevice(const QString& bluetoothDeviceId) const
{
    if (endpoints_ == nullptr || bluetoothDeviceId.isEmpty()) {
        return {};
    }
    if (!endpoints_->endpointsForBluetoothDevice(bluetoothDeviceId).isEmpty()) {
        return QStringLiteral("Available");
    }
    if (bluetoothRegistry_ == nullptr) {
        return {};
    }
    const bluetooth::BluetoothDeviceData* device = bluetoothRegistry_->findByObjectPath(bluetoothDeviceId);
    if (device == nullptr) {
        return {};
    }
    if (device->connected) {
        return QStringLiteral("Initializing...");
    }
    if (device->paired) {
        return QStringLiteral("Unavailable");
    }
    return {};
}

void PipeWireManager::refreshVirtualAudio()
{
    if (shuttingDown_) {
        return;
    }
    refreshGraph();
    virtualOutputError_.clear();
    virtualOutputProvisionAttempt_ = 0;
    virtualOutputPortWaitAttempts_ = 0;
    virtualOutputTimer_.stop();
    ensureVirtualOutput();
    bumpGraph();
}

bool PipeWireManager::openWindowsSoundSettings()
{
    struct Candidate {
        const char* program;
        QStringList args;
    };
    const Candidate candidates[] = {
        { "gnome-control-center", { QStringLiteral("sound") } },
        { "unity-control-center", { QStringLiteral("sound") } },
        { "systemsettings", { QStringLiteral("kcm_pipewire") } },
        { "systemsettings", { QStringLiteral("kcm_pulseaudio") } },
        { "pavucontrol", {} },
        { "pavucontrol-qt", {} },
    };
    for (const Candidate& candidate : candidates) {
        if (QProcess::startDetached(QString::fromLatin1(candidate.program), candidate.args)) {
            qCInfo(auralisAudio) << "LinuxSoundSettingsRequested opened=" << candidate.program;
            return true;
        }
    }
    qCWarning(auralisAudio) << "LinuxSoundSettingsRequested no known control panel found";
    return false;
}

bool PipeWireManager::initialize()
{
    if (status_ == auralis::core::ServiceStatus::Ready || status_ == auralis::core::ServiceStatus::Initializing) {
        return true;
    }

    shuttingDown_ = false;
    initialSyncEventReceived_ = false;
    status_ = auralis::core::ServiceStatus::Initializing;
    emit statusChanged();
    guard_->alive.store(true);
    const quint64 generation = guard_->generation.load() + 1;
    guard_->generation.store(generation);

    if (!startConnection(generation)) {
        status_ = auralis::core::ServiceStatus::Error;
        setConnectionState(PipeWireConnectionState::Error, QStringLiteral("PipeWire thread failed to start"));
        emit statusChanged();
        scheduleAutoReconnect(QStringLiteral("PipeWire thread failed to start"));
        return false;
    }

    return true;
}

bool PipeWireManager::startConnection(quint64 generation)
{
    return connection_->start([this, generation](const PipeWireClientEvent& event) {
        auto guard = guard_;
        QMetaObject::invokeMethod(
            this,
            [this, guard, generation, event]() { handleClientEvent(event, generation); },
            Qt::QueuedConnection);
    });
}

void PipeWireManager::setAutoReconnectEnabled(bool enabled)
{
    autoReconnectEnabled_ = enabled;
    if (!enabled) {
        reconnectTimer_.stop();
        stopInitialSyncTimeout();
    }
}

bool PipeWireManager::autoReconnectEnabled() const noexcept
{
    return autoReconnectEnabled_;
}

int PipeWireManager::reconnectAttempt() const noexcept
{
    return reconnectAttempt_;
}

int PipeWireManager::maxReconnectAttempts() const noexcept
{
    return maxReconnectAttempts_;
}

void PipeWireManager::setMaxReconnectAttemptsForTesting(int maxAttempts)
{
    maxReconnectAttempts_ = std::max(1, maxAttempts);
}

void PipeWireManager::setReconnectInitialDelayMsForTesting(int delayMs)
{
    reconnectInitialDelayMs_ = std::max(1, delayMs);
    reconnectMaxDelayMs_ = std::max(reconnectInitialDelayMs_, reconnectMaxDelayMs_);
}

void PipeWireManager::setInitialSyncTimeoutMsForTesting(int timeoutMs)
{
    initialSyncTimeoutMs_ = std::max(1, timeoutMs);
}

void PipeWireManager::fireInitialSyncTimeoutForTesting()
{
    onInitialSyncTimeout();
}

bool PipeWireManager::initialSyncTimeoutPendingForTesting() const noexcept
{
    return initialSyncTimeoutTimer_.isActive();
}

void PipeWireManager::startInitialSyncTimeout()
{
    if (shuttingDown_ || initialSyncComplete_) {
        return;
    }
    initialSyncTimeoutTimer_.start(initialSyncTimeoutMs_);
}

void PipeWireManager::stopInitialSyncTimeout()
{
    initialSyncTimeoutTimer_.stop();
}

void PipeWireManager::onInitialSyncTimeout()
{
    if (shuttingDown_ || initialSyncComplete_) {
        return;
    }
    if (connectionState_ != PipeWireConnectionState::Connected) {
        return;
    }
    qCWarning(auralisAudio) << "PipeWire initial graph sync timed out";
    stopInitialSyncTimeout();
    reconnectInProgress_ = false;
    initialSyncComplete_ = false;
    initialSyncEventReceived_ = false;
    if (router_ != nullptr) {
        router_->handleConnectionState(PipeWireConnectionState::Error, false);
    }
    connection_->stop();
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    }
    endpoints_->clear();
    store_->clear();
    resetVirtualOutputState();
    status_ = auralis::core::ServiceStatus::Error;
    setConnectionState(PipeWireConnectionState::Error, QStringLiteral("PipeWire initial graph sync timed out"));
    emit statusChanged();
    bumpGraph();
    scheduleAutoReconnect(QStringLiteral("PipeWire initial graph sync timed out"));
}

void PipeWireManager::injectClientEventForTesting(const PipeWireClientEvent& event)
{
    handleClientEvent(event, guard_->generation.load());
}

void PipeWireManager::simulateReconnectFailureForTesting(const QString& reason)
{
    if (shuttingDown_) {
        return;
    }
    reconnectTimer_.stop();
    reconnectInProgress_ = false;
    initialSyncComplete_ = false;
    scheduleAutoReconnect(reason.isEmpty() ? QStringLiteral("test") : reason);
    reconnectTimer_.stop();
}

void PipeWireManager::requestReconnect()
{
    if (shuttingDown_) {
        return;
    }
    // Manual reconnect starts a fresh episode after exhaustion.
    if (reconnectExhaustedEmitted_ || reconnectAttempt_ >= maxReconnectAttempts_) {
        reconnectAttempt_ = 0;
        reconnectExhaustedEmitted_ = false;
    }
    reconnectTimer_.stop();
    performReconnect();
}

void PipeWireManager::scheduleAutoReconnect(const QString& reason)
{
    if (shuttingDown_ || !autoReconnectEnabled_ || reconnectTimer_.isActive()) {
        return;
    }
    if (reconnectAttempt_ >= maxReconnectAttempts_) {
        if (!reconnectExhaustedEmitted_) {
            reconnectExhaustedEmitted_ = true;
            qCWarning(auralisAudio) << "PipeWire reconnect exhausted:" << reason;
            emit reconnectExhausted(reason);
        }
        return;
    }
    ++reconnectAttempt_;
    int delay = reconnectInitialDelayMs_;
    for (int i = 1; i < reconnectAttempt_; ++i) {
        delay = std::min(delay * 2, reconnectMaxDelayMs_);
    }
    qCInfo(auralisAudio) << "PipeWire reconnect scheduled attempt=" << reconnectAttempt_ << "delayMs=" << delay
                          << "reason=" << reason;
    emit reconnectAttemptStarted(reconnectAttempt_);
    reconnectTimer_.start(delay);
}

void PipeWireManager::performReconnect()
{
    if (shuttingDown_) {
        return;
    }
    reconnectInProgress_ = true;
    qCInfo(auralisAudio) << "PipeWire reconnect attempt=" << reconnectAttempt_;
    graphRefreshTimer_.stop();
    if (router_ != nullptr) {
        router_->handleConnectionState(PipeWireConnectionState::Stopped, false);
    }
    connection_->stop();
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    }
    endpoints_->clear();
    store_->clear();
    resetVirtualOutputState();
    initialSyncComplete_ = false;
    initialSyncEventReceived_ = false;
    lastError_.clear();

    const quint64 generation = guard_->generation.fetch_add(1) + 1;
    guard_->alive.store(true);
    status_ = auralis::core::ServiceStatus::Initializing;
    setConnectionState(PipeWireConnectionState::Starting);
    emit statusChanged();

    if (!startConnection(generation)) {
        reconnectInProgress_ = false;
        status_ = auralis::core::ServiceStatus::Error;
        setConnectionState(PipeWireConnectionState::Error, QStringLiteral("PipeWire reconnect failed to start"));
        emit statusChanged();
        scheduleAutoReconnect(QStringLiteral("PipeWire reconnect failed to start"));
        return;
    }
    reconnectInProgress_ = false;
}

void PipeWireManager::shutdown()
{
    if (status_ == auralis::core::ServiceStatus::Uninitialized) {
        return;
    }

    shuttingDown_ = true;
    qCInfo(auralisAudio) << "PipeWire Stopping";
    reconnectTimer_.stop();
    stopInitialSyncTimeout();
    graphRefreshTimer_.stop();
    virtualOutputTimer_.stop();
    guard_->alive.store(false);
    guard_->generation.fetch_add(1);
    if (router_ != nullptr) {
        router_->shutdown();
    }
    connection_->stop();
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    }
    endpoints_->clear();
    store_->clear();
    resetVirtualOutputState();
    initialSyncComplete_ = false;
    initialSyncEventReceived_ = false;
    lastError_.clear();
    reconnectAttempt_ = 0;
    reconnectExhaustedEmitted_ = false;
    setConnectionState(PipeWireConnectionState::Stopped);
    status_ = auralis::core::ServiceStatus::Uninitialized;
    bumpGraph();
    emit statusChanged();
    qCInfo(auralisAudio) << "PipeWire manager shut down";
}

void PipeWireManager::handleClientEvent(const PipeWireClientEvent& event, quint64 generation)
{
    if (!guard_->alive.load() || guard_->generation.load() != generation) {
        return;
    }

    switch (event.type) {
    case PipeWireClientEvent::Type::StateChanged:
        setConnectionState(event.state, event.error);
        if (event.state == PipeWireConnectionState::Connected) {
            status_ = auralis::core::ServiceStatus::Ready;
            reconnectTimer_.stop();
            emit statusChanged();
            if (router_ != nullptr) {
                router_->handleConnectionState(event.state, initialSyncComplete_);
            }
            // Do not reset reconnectAttempt_ until InitialSyncDone (usable graph).
            if (initialSyncEventReceived_) {
                completeInitialSync();
            } else if (!initialSyncComplete_) {
                startInitialSyncTimeout();
            }
        } else if (event.state == PipeWireConnectionState::Error
                   || event.state == PipeWireConnectionState::Stopped) {
            stopInitialSyncTimeout();
            status_ = auralis::core::ServiceStatus::Error;
            if (router_ != nullptr) {
                router_->handleConnectionState(event.state, false);
            }
            endpoints_->clear();
            store_->clear();
            resetVirtualOutputState();
            initialSyncComplete_ = false;
            initialSyncEventReceived_ = false;
            emit statusChanged();
            bumpGraph();
            if (!shuttingDown_ && !reconnectInProgress_) {
                scheduleAutoReconnect(
                    event.error.isEmpty() ? toString(event.state) : event.error);
            }
        }
        break;
    case PipeWireClientEvent::Type::GlobalAdded:
        qCDebug(auralisAudio) << "PipeWireRegistry GlobalAdded id=" << event.snapshot.globalId
                              << "type=" << event.snapshot.interfaceType
                              << "mediaClass=" << event.snapshot.properties.value(QStringLiteral("media.class"));
        if (store_->upsert(event.snapshot) && initialSyncComplete_) {
            scheduleGraphRefresh();
        }
        break;
    case PipeWireClientEvent::Type::GlobalUpdated:
        if (store_->upsert(event.snapshot) && initialSyncComplete_) {
            scheduleGraphRefresh();
        }
        break;
    case PipeWireClientEvent::Type::GlobalRemoved:
        qCDebug(auralisAudio) << "PipeWireRegistry GlobalRemoved id=" << event.removedId;
        if (store_->remove(event.removedId) && initialSyncComplete_) {
            scheduleGraphRefresh();
        }
        break;
    case PipeWireClientEvent::Type::InitialSyncDone:
        initialSyncEventReceived_ = true;
        if (connectionState_ != PipeWireConnectionState::Connected) {
            break;
        }
        completeInitialSync();
        break;
    case PipeWireClientEvent::Type::MetadataChanged:
        if (event.metadataSubject == 0
            && event.metadataName == QLatin1String("default")
            && event.metadataKey == QLatin1String("default.audio.sink")) {
            const QString nextDefault = pipeWireDefaultNodeName(event.metadataValue);
            if (defaultAudioSinkName_ != nextDefault) {
                defaultAudioSinkName_ = nextDefault;
                updateVirtualOutputState();
                bumpGraph();
                qCInfo(auralisAudio) << "PipeWire DefaultAudioSinkChanged name=" << defaultAudioSinkName_;
            }
        }
        break;
    }
}

void PipeWireManager::setConnectionState(PipeWireConnectionState state, const QString& error)
{
    bool changed = false;
    if (connectionState_ != state) {
        connectionState_ = state;
        changed = true;
        emit connectionStateChanged();
    }
    if (lastError_ != error) {
        lastError_ = error;
        emit lastErrorChanged();
        changed = true;
    }
    if (changed) {
        bumpGraph();
    }
}

void PipeWireManager::scheduleGraphRefresh()
{
    if (shuttingDown_ || !initialSyncComplete_ || graphRefreshTimer_.isActive()) {
        return;
    }
    graphRefreshTimer_.start();
}

void PipeWireManager::refreshGraph()
{
    if (store_ == nullptr || endpoints_ == nullptr || resolver_ == nullptr) {
        return;
    }
    refreshEndpointsFromStore(*store_, *endpoints_, *resolver_);
    if (router_ != nullptr) {
        router_->handleGraphChanged();
    }
    updateVirtualOutputState();
    bumpGraph();
}

void PipeWireManager::completeInitialSync()
{
    if (initialSyncComplete_ || !initialSyncEventReceived_
        || connectionState_ != PipeWireConnectionState::Connected) {
        return;
    }
    stopInitialSyncTimeout();
    initialSyncComplete_ = true;
    graphRefreshTimer_.stop();
    refreshGraph();
    reconnectAttempt_ = 0;
    reconnectExhaustedEmitted_ = false;
    reconnectTimer_.stop();
    if (router_ != nullptr) {
        router_->handleConnectionState(connectionState_, true);
    }
    ensureVirtualOutput();
    qCInfo(auralisAudio) << "PipeWire initial registry sync complete" << diagnosticsText();
}

void PipeWireManager::updateVirtualOutputState()
{
    if (store_ == nullptr) {
        virtualOutput_ = {};
        return;
    }
    const bool wasReady = virtualOutput_.ready();
    const bool wasSelected = virtualOutput_.selectedAsDefault;
    virtualOutput_ = inspectPipeWireVirtualOutput(*store_, defaultAudioSinkName_);
    if (virtualOutput_.ready()) {
        virtualOutputTimer_.stop();
        virtualOutputProvisionAttempt_ = 0;
        virtualOutputPortWaitAttempts_ = 0;
        virtualOutputError_.clear();
        if (!wasReady || wasSelected != virtualOutput_.selectedAsDefault) {
            qCInfo(auralisAudio) << "PipeWire VirtualOutputReady selected=" << virtualOutput_.selectedAsDefault
                                 << "persistent=" << virtualOutput_.packageManaged;
        }
        return;
    }
    if (initialSyncComplete_ && connected() && !virtualOutputTimer_.isActive()) {
        virtualOutputTimer_.start(500);
    }
}

void PipeWireManager::ensureVirtualOutput()
{
    if (shuttingDown_ || !initialSyncComplete_ || !connected() || connection_ == nullptr) {
        return;
    }
    updateVirtualOutputState();
    if (virtualOutput_.ready()) {
        bumpGraph();
        return;
    }

    const bool ownsRuntime = connection_->ownsVirtualOutput();
    if (virtualOutput_.partial() && !virtualOutput_.ready()) {
        // Adapter ports often arrive after the loopback nodes. Wait before
        // tearing down a runtime module or declaring a package drop-in dead.
        constexpr int kMaxPortWaitAttempts = 8;
        if (virtualOutputPortWaitAttempts_ < kMaxPortWaitAttempts) {
            ++virtualOutputPortWaitAttempts_;
            virtualOutputTimer_.start(500);
            bumpGraph();
            return;
        }
        virtualOutputPortWaitAttempts_ = 0;
        if (!ownsRuntime) {
            virtualOutputTimer_.stop();
            virtualOutputError_ =
                QStringLiteral("persistent PipeWire node is incomplete; restart the user audio service");
            qCWarning(auralisAudio) << "PipeWire VirtualOutputIncomplete persistence=package";
            bumpGraph();
            return;
        }
    }

    if (virtualOutputProvisionAttempt_ >= 3) {
        virtualOutputTimer_.stop();
        virtualOutputError_ = QStringLiteral("PipeWire loopback module could not be provisioned");
        qCWarning(auralisAudio) << "PipeWire VirtualOutputProvisionExhausted attempts="
                                << virtualOutputProvisionAttempt_;
        bumpGraph();
        return;
    }

    if (ownsRuntime) {
        connection_->destroyVirtualOutput();
    }

    QString error;
    ++virtualOutputProvisionAttempt_;
    if (!connection_->createVirtualOutput(&error)) {
        virtualOutputError_ = error;
        qCWarning(auralisAudio) << "PipeWire VirtualOutputUnavailable" << error;
        if (virtualOutputProvisionAttempt_ < 3) {
            const int retryDelayMs = virtualOutputProvisionAttempt_ * 1000;
            qCInfo(auralisAudio) << "PipeWire VirtualOutputRetryScheduled attempt="
                                 << virtualOutputProvisionAttempt_ << "delayMs=" << retryDelayMs;
            virtualOutputTimer_.start(retryDelayMs);
        }
        bumpGraph();
        return;
    }
    virtualOutputError_.clear();
    virtualOutputPortWaitAttempts_ = 0;
    virtualOutputTimer_.start(1500);
    bumpGraph();
}

void PipeWireManager::resetVirtualOutputState()
{
    virtualOutputTimer_.stop();
    virtualOutput_ = {};
    defaultAudioSinkName_.clear();
    virtualOutputError_.clear();
    virtualOutputProvisionAttempt_ = 0;
    virtualOutputPortWaitAttempts_ = 0;
}

void PipeWireManager::bumpGraph()
{
    ++graphRevision_;
    emit graphRevisionChanged();
}

} // namespace auralis::audio

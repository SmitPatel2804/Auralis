#include <auralis/audio/PipeWireManager.h>

#include <auralis/audio/AudioEndpointListModel.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/EndpointResolver.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/LoggingCategories.h>

#include <QCoreApplication>

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
    graphRefreshTimer_.setInterval(100);
    connect(&graphRefreshTimer_, &QTimer::timeout, this, &PipeWireManager::refreshGraph);
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
    const auto refresh = [this]() { refreshGraph(); };
    connect(bluetoothRegistry_, &bluetooth::DeviceRegistry::deviceAdded, this, refresh);
    connect(bluetoothRegistry_, &bluetooth::DeviceRegistry::deviceUpdated, this, refresh);
    connect(bluetoothRegistry_, &bluetooth::DeviceRegistry::deviceRemoved, this, refresh);
}

QObject* PipeWireManager::uiObject()
{
    return this;
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
               "state=%1 devices=%2 nodes=%3 ports=%4 links=%5 endpoints=%6 mappedBt=%7 unmappedBt=%8 error=%9")
        .arg(
            toString(connectionState_),
            QString::number(store_->deviceCount()),
            QString::number(store_->nodeCount()),
            QString::number(store_->portCount()),
            QString::number(store_->linkCount()),
            QString::number(endpoints_->count()),
            QString::number(endpoints_->mappedBluetoothEndpoints().size()),
            QString::number(endpoints_->unmappedBluetoothEndpoints().size()),
            lastError_.isEmpty() ? QStringLiteral("none") : lastError_);
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

bool PipeWireManager::initialize()
{
    if (status_ == auralis::core::ServiceStatus::Ready || status_ == auralis::core::ServiceStatus::Initializing) {
        return true;
    }

    status_ = auralis::core::ServiceStatus::Initializing;
    emit statusChanged();
    guard_->alive.store(true);
    const quint64 generation = guard_->generation.load() + 1;
    guard_->generation.store(generation);

    const bool started = connection_->start([this, generation](const PipeWireClientEvent& event) {
        auto guard = guard_;
        QMetaObject::invokeMethod(
            this,
            [this, guard, generation, event]() { handleClientEvent(event, generation); },
            Qt::QueuedConnection);
    });

    if (!started) {
        status_ = auralis::core::ServiceStatus::Error;
        setConnectionState(PipeWireConnectionState::Error, QStringLiteral("PipeWire thread failed to start"));
        emit statusChanged();
        return false;
    }

    return true;
}

void PipeWireManager::shutdown()
{
    if (status_ == auralis::core::ServiceStatus::Uninitialized) {
        return;
    }

    qCInfo(auralisAudio) << "PipeWire Stopping";
    graphRefreshTimer_.stop();
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
    initialSyncComplete_ = false;
    lastError_.clear();
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
            emit statusChanged();
            if (router_ != nullptr) {
                router_->handleConnectionState(event.state, initialSyncComplete_);
            }
        } else if (event.state == PipeWireConnectionState::Error) {
            status_ = auralis::core::ServiceStatus::Error;
            if (router_ != nullptr) {
                router_->handleConnectionState(event.state, false);
            }
            endpoints_->clear();
            store_->clear();
            initialSyncComplete_ = false;
            emit statusChanged();
            bumpGraph();
        }
        break;
    case PipeWireClientEvent::Type::GlobalAdded:
        qCDebug(auralisAudio) << "PipeWireRegistry GlobalAdded id=" << event.snapshot.globalId
                              << "type=" << event.snapshot.interfaceType
                              << "mediaClass=" << event.snapshot.properties.value(QStringLiteral("media.class"));
        store_->upsert(event.snapshot);
        if (initialSyncComplete_) {
            scheduleGraphRefresh();
        }
        break;
    case PipeWireClientEvent::Type::GlobalUpdated:
        store_->upsert(event.snapshot);
        if (initialSyncComplete_) {
            scheduleGraphRefresh();
        }
        break;
    case PipeWireClientEvent::Type::GlobalRemoved:
        qCDebug(auralisAudio) << "PipeWireRegistry GlobalRemoved id=" << event.removedId;
        store_->remove(event.removedId);
        if (initialSyncComplete_) {
            scheduleGraphRefresh();
        }
        break;
    case PipeWireClientEvent::Type::InitialSyncDone:
        initialSyncComplete_ = true;
        graphRefreshTimer_.stop();
        refreshGraph();
        if (router_ != nullptr) {
            router_->handleConnectionState(connectionState_, true);
        }
        qCInfo(auralisAudio) << "PipeWire initial registry sync complete" << diagnosticsText();
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
    bumpGraph();
}

void PipeWireManager::bumpGraph()
{
    ++graphRevision_;
    emit graphRevisionChanged();
}

} // namespace auralis::audio

#include <auralis/audio/PipeWireManager.h>

#include <auralis/audio/AudioEndpointListModel.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/EndpointResolver.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/LoggingCategories.h>

#include <QCoreApplication>

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
    graphRefreshTimer_.setInterval(100);
    connect(&graphRefreshTimer_, &QTimer::timeout, this, &PipeWireManager::refreshGraph);

    reconnectTimer_.setSingleShot(true);
    connect(&reconnectTimer_, &QTimer::timeout, this, &PipeWireManager::performReconnect);
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

    shuttingDown_ = false;
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
    initialSyncComplete_ = false;
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
        } else if (event.state == PipeWireConnectionState::Error
                   || event.state == PipeWireConnectionState::Stopped) {
            status_ = auralis::core::ServiceStatus::Error;
            if (router_ != nullptr) {
                router_->handleConnectionState(event.state, false);
            }
            endpoints_->clear();
            store_->clear();
            initialSyncComplete_ = false;
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
        if (connectionState_ == PipeWireConnectionState::Connected) {
            reconnectAttempt_ = 0;
            reconnectExhaustedEmitted_ = false;
            reconnectTimer_.stop();
        }
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

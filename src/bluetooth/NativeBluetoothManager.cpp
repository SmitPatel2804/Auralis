#include <auralis/bluetooth/NativeBluetoothManager.h>

#include <auralis/bluetooth/BlueZTypes.h>
#include <auralis/bluetooth/BluetoothDeviceListModel.h>
#include <auralis/bluetooth/DeviceOperation.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/UuidCatalog.h>
#include <auralis/core/LoggingCategories.h>

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothLocalDevice>
#include <QBluetoothUuid>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QTimer>

namespace auralis::bluetooth {
namespace {

QString stableNativeId(const QBluetoothDeviceInfo& info)
{
    if (!info.address().isNull()) {
        QString token = info.address().toString();
        token.remove(QLatin1Char(':'));
        return QStringLiteral("native:%1").arg(token.toUpper());
    }
    if (!info.deviceUuid().isNull()) {
        return QStringLiteral("native:%1").arg(info.deviceUuid().toString(QUuid::WithoutBraces).toUpper());
    }
    return {};
}

QString persistentNativeAddress(const QBluetoothDeviceInfo& info)
{
    if (!info.address().isNull()) {
        return info.address().toString().toUpper();
    }
    return info.deviceUuid().toString(QUuid::WithoutBraces).toUpper();
}

QStringList serviceIds(const QBluetoothDeviceInfo& info)
{
    QStringList result;
    for (const QBluetoothUuid& uuid : info.serviceUuids()) {
        result.push_back(uuid.toString().toLower());
    }
    return result;
}

} // namespace

struct NativeBluetoothManager::State {
    QBluetoothLocalDevice* local = nullptr;
    QBluetoothDeviceDiscoveryAgent* discovery = nullptr;
    QTimer connectionPoll;
    QHash<QString, QBluetoothAddress> addressById;
    QSet<QString> reconnectSuppressed;
    bool scanning = false;
};

NativeBluetoothManager::NativeBluetoothManager(QObject* parent)
    : QObject(parent)
    , stateImpl_(std::make_unique<State>())
    , registry_(new DeviceRegistry(this))
    , model_(new BluetoothDeviceListModel(registry_, this))
    , reconnect_(new ReconnectPolicy(this))
{
    connect(registry_, &DeviceRegistry::countChanged, this, &NativeBluetoothManager::deviceCountChanged);
    connect(registry_, &DeviceRegistry::deviceRemoved, this, &NativeBluetoothManager::connectedDeviceCountChanged);
    connect(this, &NativeBluetoothManager::availableChanged, this, &NativeBluetoothManager::statusTextChanged);
    connect(this, &NativeBluetoothManager::adapterChanged, this, &NativeBluetoothManager::statusTextChanged);
    connect(this, &NativeBluetoothManager::scanningChanged, this, &NativeBluetoothManager::statusTextChanged);
    connect(reconnect_, &ReconnectPolicy::reconnectDue, this, [this](const QString& id, int, int) {
        connectDevice(id);
        reconnect_->completeReconnectAttempt(id);
        updateDeviceConnectionState();
        const auto* device = registry_->findByObjectPath(id);
        if (device != nullptr && device->connected) {
            reconnect_->onConnected(id);
        } else if (!stateImpl_->reconnectSuppressed.contains(id)) {
            reconnect_->scheduleReconnect(id);
        }
    });
    connect(reconnect_, &ReconnectPolicy::reconnectExhausted, this, &NativeBluetoothManager::managedReconnectExhausted);
    connect(reconnect_, &ReconnectPolicy::reconnectTerminalFailure, this, &NativeBluetoothManager::managedReconnectTerminalFailure);
}

NativeBluetoothManager::~NativeBluetoothManager()
{
    shutdown();
}

bool NativeBluetoothManager::initialize()
{
    if (status_ == core::ServiceStatus::Ready) {
        return true;
    }
    status_ = core::ServiceStatus::Initializing;
    stateImpl_->local = new QBluetoothLocalDevice(this);
    stateImpl_->discovery = new QBluetoothDeviceDiscoveryAgent(this);
    connect(stateImpl_->discovery, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, [this](const QBluetoothDeviceInfo& info) {
        const QString id = stableNativeId(info);
        if (id.isEmpty()) {
            return;
        }
        BluetoothDeviceData device;
        device.objectPath = id;
        device.adapterPath = QStringLiteral("native:default-adapter");
        device.address = persistentNativeAddress(info);
        device.addressType = info.address().isNull() ? QStringLiteral("platform-uuid") : QStringLiteral("public");
        device.name = info.name();
        device.alias = info.name();
        device.rssi = info.rssi();
        device.hasRssi = info.rssi() != 0;
        device.uuids = serviceIds(info);
        device.lastSeen = QDateTime::currentDateTimeUtc();
        device.servicesResolved = !device.uuids.isEmpty();
        if (!info.address().isNull() && stateImpl_->local->isValid()) {
            const auto pairing = stateImpl_->local->pairingStatus(info.address());
            device.paired = pairing != QBluetoothLocalDevice::Unpaired;
            device.trusted = pairing == QBluetoothLocalDevice::AuthorizedPaired;
            stateImpl_->addressById.insert(id, info.address());
        }
        registry_->upsertDevice(device);
        updateDeviceConnectionState();
    });
    const auto scanFinished = [this]() {
        if (!stateImpl_->scanning) {
            return;
        }
        stateImpl_->scanning = false;
        emit scanningChanged();
        emit adapterChanged();
        updateDeviceConnectionState();
    };
    connect(stateImpl_->discovery, &QBluetoothDeviceDiscoveryAgent::finished, this, scanFinished);
    connect(stateImpl_->discovery, &QBluetoothDeviceDiscoveryAgent::canceled, this, scanFinished);
    connect(
        stateImpl_->discovery,
        &QBluetoothDeviceDiscoveryAgent::errorOccurred,
        this,
        [this](QBluetoothDeviceDiscoveryAgent::Error) {
            if (stateImpl_->scanning) {
                stateImpl_->scanning = false;
                emit scanningChanged();
                emit adapterChanged();
            }
            setError(stateImpl_->discovery->errorString());
        });
    connect(stateImpl_->local, &QBluetoothLocalDevice::hostModeStateChanged, this, [this]() {
        emit adapterChanged();
        emit availableChanged();
        emit transportConnectedChanged(transportConnected());
    });
    connect(
        stateImpl_->local,
        &QBluetoothLocalDevice::errorOccurred,
        this,
        [this](QBluetoothLocalDevice::Error) {
            setError(QStringLiteral("The operating system rejected the Bluetooth operation."));
        });
    connect(stateImpl_->local, &QBluetoothLocalDevice::deviceConnected, this, [this](const QBluetoothAddress&) {
        updateDeviceConnectionState();
    });
    connect(stateImpl_->local, &QBluetoothLocalDevice::deviceDisconnected, this, [this](const QBluetoothAddress&) {
        updateDeviceConnectionState();
    });
    connect(
        stateImpl_->local,
        &QBluetoothLocalDevice::pairingFinished,
        this,
        [this](const QBluetoothAddress& address, QBluetoothLocalDevice::Pairing pairing) {
            for (auto it = stateImpl_->addressById.cbegin(); it != stateImpl_->addressById.cend(); ++it) {
                if (it.value() != address) {
                    continue;
                }
                registry_->updateDevice(it.key(), [pairing](BluetoothDeviceData& device) {
                    device.paired = pairing != QBluetoothLocalDevice::Unpaired;
                    device.trusted = pairing == QBluetoothLocalDevice::AuthorizedPaired;
                    device.operation = DeviceOperation::Idle;
                    device.lastErrorMessage.clear();
                });
                break;
            }
            updateDeviceConnectionState();
        });
    stateImpl_->connectionPoll.setInterval(1500);
    connect(&stateImpl_->connectionPoll, &QTimer::timeout, this, &NativeBluetoothManager::updateDeviceConnectionState);
    stateImpl_->connectionPoll.start();
    status_ = core::ServiceStatus::Ready;
    emit availableChanged();
    emit adapterChanged();
    emit statusTextChanged();
    emit transportConnectedChanged(transportConnected());
    return true;
}

void NativeBluetoothManager::shutdown()
{
    if (status_ == core::ServiceStatus::Uninitialized) {
        return;
    }
    stopScan();
    stateImpl_->connectionPoll.stop();
    reconnect_->cancelAll();
    registry_->clear();
    if (stateImpl_->discovery != nullptr) {
        stateImpl_->discovery->deleteLater();
        stateImpl_->discovery = nullptr;
    }
    if (stateImpl_->local != nullptr) {
        stateImpl_->local->deleteLater();
        stateImpl_->local = nullptr;
    }
    stateImpl_->addressById.clear();
    status_ = core::ServiceStatus::Uninitialized;
}

core::ServiceStatus NativeBluetoothManager::status() const noexcept { return status_; }
QObject* NativeBluetoothManager::uiObject() { return this; }
QString NativeBluetoothManager::backendName() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Windows Bluetooth");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("Core Bluetooth");
#else
    return QStringLiteral("Qt Bluetooth");
#endif
}
bool NativeBluetoothManager::available() const noexcept { return stateImpl_->local != nullptr && stateImpl_->local->isValid(); }
bool NativeBluetoothManager::transportConnected() const noexcept { return available(); }
bool NativeBluetoothManager::adapterPresent() const noexcept { return available(); }
DeviceRegistry* NativeBluetoothManager::deviceRegistry() const noexcept { return registry_; }
bool NativeBluetoothManager::adapterPowered() const
{
    return available() && stateImpl_->local->hostMode() != QBluetoothLocalDevice::HostPoweredOff;
}
bool NativeBluetoothManager::scanning() const noexcept { return stateImpl_->scanning; }
bool NativeBluetoothManager::canStartScan() const { return adapterPowered() && !scanning(); }
bool NativeBluetoothManager::canStopScan() const { return scanning(); }
int NativeBluetoothManager::deviceCount() const { return registry_->count(); }
int NativeBluetoothManager::connectedDeviceCount() const
{
    int count = 0;
    for (const BluetoothDeviceData& device : registry_->devices()) {
        count += device.connected ? 1 : 0;
    }
    return count;
}
QString NativeBluetoothManager::statusText() const
{
    if (!available()) return QStringLiteral("Bluetooth adapter unavailable");
    if (!adapterPowered()) return QStringLiteral("Bluetooth powered off");
    if (scanning()) return QStringLiteral("Scanning");
    return QStringLiteral("Ready");
}
QString NativeBluetoothManager::errorText() const { return errorText_; }
QString NativeBluetoothManager::adapterName() const { return available() ? stateImpl_->local->name() : QString(); }
QString NativeBluetoothManager::adapterAddress() const
{
    return available() ? stateImpl_->local->address().toString() : QString();
}
QAbstractItemModel* NativeBluetoothManager::devices() const { return model_; }
QObject* NativeBluetoothManager::pendingPairingRequest() const { return nullptr; }
bool NativeBluetoothManager::agentRegistered() const { return false; }

void NativeBluetoothManager::startScan()
{
    if (!canStartScan()) return;
    const auto methods = QBluetoothDeviceDiscoveryAgent::supportedDiscoveryMethods();
    if (methods == QBluetoothDeviceDiscoveryAgent::NoMethod) {
        setError(QStringLiteral("Bluetooth discovery is not supported by this operating-system backend."));
        return;
    }
    clearError();
    stateImpl_->scanning = true;
    emit scanningChanged();
    emit adapterChanged();
    stateImpl_->discovery->start(methods);
}
void NativeBluetoothManager::stopScan()
{
    if (stateImpl_->discovery != nullptr && stateImpl_->discovery->isActive()) {
        stateImpl_->discovery->stop();
    }
    if (stateImpl_->scanning) {
        stateImpl_->scanning = false;
        emit scanningChanged();
        emit adapterChanged();
    }
}
void NativeBluetoothManager::refresh()
{
    if (stateImpl_->local != nullptr && !stateImpl_->local->isValid()) {
        shutdown();
        initialize();
        return;
    }
    updateDeviceConnectionState();
    if (!scanning()) startScan();
}

void NativeBluetoothManager::pairDevice(const QString& id)
{
    const QBluetoothAddress address = stateImpl_->addressById.value(id);
    if (!available() || address.isNull()) {
        setError(QStringLiteral("This device must be paired through the operating-system Bluetooth settings."));
        return;
    }
    registry_->setOperation(id, DeviceOperation::Pairing);
    stateImpl_->local->requestPairing(address, QBluetoothLocalDevice::Paired);
}
void NativeBluetoothManager::cancelPairing(const QString& id) { cancelDeviceOperation(id); }
void NativeBluetoothManager::trustDevice(const QString& id)
{
    const QBluetoothAddress address = stateImpl_->addressById.value(id);
    if (!available() || address.isNull()) return;
    registry_->setOperation(id, DeviceOperation::Trusting);
    stateImpl_->local->requestPairing(address, QBluetoothLocalDevice::AuthorizedPaired);
}
void NativeBluetoothManager::untrustDevice(const QString& id)
{
    const QBluetoothAddress address = stateImpl_->addressById.value(id);
    if (!available() || address.isNull()) return;
    registry_->setOperation(id, DeviceOperation::Trusting);
    stateImpl_->local->requestPairing(address, QBluetoothLocalDevice::Paired);
}
void NativeBluetoothManager::connectDevice(const QString& id)
{
    const auto* device = registry_->findByObjectPath(id);
    if (device == nullptr) return;
    if (!device->paired) {
        pairDevice(id);
        return;
    }
    registry_->setOperation(id, DeviceOperation::Connecting);
    updateDeviceConnectionState();
    if (const auto* updated = registry_->findByObjectPath(id); updated == nullptr || !updated->connected) {
        registry_->setLastError(
            id,
            BluetoothError::OperationFailed,
            QStringLiteral("NativeProfileConnectionRequired"),
            QStringLiteral("The operating system has not exposed an active Bluetooth audio profile yet."));
        registry_->clearOperation(id);
    }
}
void NativeBluetoothManager::disconnectDevice(const QString& id)
{
    registry_->setUserDisconnectRequested(id, true);
    registry_->setLastError(
        id,
        BluetoothError::OperationFailed,
        QStringLiteral("NativeProfileDisconnectRequired"),
        QStringLiteral("Disconnect this audio profile from the operating-system Bluetooth settings."));
}
void NativeBluetoothManager::forgetDevice(const QString& id)
{
    const QBluetoothAddress address = stateImpl_->addressById.value(id);
    if (!available() || address.isNull()) return;
    stateImpl_->local->requestPairing(address, QBluetoothLocalDevice::Unpaired);
}
void NativeBluetoothManager::reconnectDevice(const QString& id) { connectDevice(id); }
void NativeBluetoothManager::cancelDeviceOperation(const QString& id) { registry_->clearOperation(id); }
QString NativeBluetoothManager::serviceFriendlyName(const QString& uuid) const { return UuidCatalog::friendlyName(uuid); }
bool NativeBluetoothManager::userDisconnectRequestedForDevice(const QString& id) const
{
    const auto* device = registry_->findByObjectPath(id);
    return device != nullptr && device->userDisconnectRequested;
}
QString NativeBluetoothManager::deviceDisplayName(const QString& id) const
{
    const auto* device = registry_->findByObjectPath(id);
    return device != nullptr ? device->displayName() : id;
}
bool NativeBluetoothManager::hasDevice(const QString& id) const { return registry_->findByObjectPath(id) != nullptr; }
QVariantMap NativeBluetoothManager::deviceDetails(const QString& id) const
{
    QVariantMap result;
    const auto* device = registry_->findByObjectPath(id);
    if (device == nullptr) return result;
    result.insert(QStringLiteral("objectPath"), device->objectPath);
    result.insert(QStringLiteral("name"), device->name);
    result.insert(QStringLiteral("alias"), device->alias);
    result.insert(QStringLiteral("displayName"), device->displayName());
    result.insert(QStringLiteral("address"), device->address);
    result.insert(QStringLiteral("addressType"), device->addressType);
    result.insert(QStringLiteral("paired"), device->paired);
    result.insert(QStringLiteral("trusted"), device->trusted);
    result.insert(QStringLiteral("connected"), device->connected);
    result.insert(QStringLiteral("servicesResolved"), device->servicesResolved);
    result.insert(QStringLiteral("uuids"), device->uuids);
    result.insert(QStringLiteral("transport"), device->transportHint());
    return result;
}

void NativeBluetoothManager::requestManagedReconnect(const QString& id)
{
    if (!stateImpl_->reconnectSuppressed.contains(id)) reconnect_->scheduleReconnect(id);
}
void NativeBluetoothManager::cancelManagedReconnect(const QString& id) { reconnect_->cancelReconnect(id); }
void NativeBluetoothManager::suppressAutoReconnect(const QString& id)
{
    stateImpl_->reconnectSuppressed.insert(id);
    reconnect_->cancelReconnect(id);
}
void NativeBluetoothManager::unsuppressAutoReconnect(const QString& id) { stateImpl_->reconnectSuppressed.remove(id); }
void NativeBluetoothManager::pauseManagedReconnect() { reconnect_->pauseAll(); }
void NativeBluetoothManager::resumeManagedReconnect() { reconnect_->resumeAll(); }

void NativeBluetoothManager::updateDeviceConnectionState()
{
    if (!available()) return;
    const QList<QBluetoothAddress> connectedList = stateImpl_->local->connectedDevices();
    const QSet<QBluetoothAddress> connected(connectedList.cbegin(), connectedList.cend());
    bool connectionCountChanged = false;
    for (auto it = stateImpl_->addressById.cbegin(); it != stateImpl_->addressById.cend(); ++it) {
        const bool isConnected = connected.contains(it.value());
        const BluetoothDeviceData* current = registry_->findByObjectPath(it.key());
        if (current == nullptr) {
            continue;
        }
        const bool wasConnected = current->connected;
        const bool needsUpdate = wasConnected != isConnected
            || (isConnected
                && (!current->servicesResolved
                    || current->operation != DeviceOperation::Idle
                    || current->userDisconnectRequested));
        if (needsUpdate) {
            registry_->updateDevice(it.key(), [isConnected](BluetoothDeviceData& device) {
                device.connected = isConnected;
                if (isConnected) {
                    device.servicesResolved = true;
                    device.operation = DeviceOperation::Idle;
                    device.userDisconnectRequested = false;
                }
            });
        }
        if (wasConnected != isConnected) {
            connectionCountChanged = true;
        }
        if (isConnected && !wasConnected) {
            reconnect_->onConnected(it.key());
        }
    }
    if (connectionCountChanged) {
        emit connectedDeviceCountChanged();
    }
}

void NativeBluetoothManager::setError(const QString& text)
{
    if (errorText_ == text) return;
    errorText_ = text;
    emit errorTextChanged();
    emit statusTextChanged();
}
void NativeBluetoothManager::clearError()
{
    if (errorText_.isEmpty()) return;
    errorText_.clear();
    emit errorTextChanged();
}

} // namespace auralis::bluetooth

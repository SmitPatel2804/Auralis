#include <auralis/bluetooth/DeviceLifecycleManager.h>

#include <auralis/bluetooth/AdapterManager.h>
#include <auralis/bluetooth/BlueZAgent.h>
#include <auralis/bluetooth/BlueZConstants.h>
#include <auralis/bluetooth/BluetoothDbusError.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/IBlueZClient.h>
#include <auralis/bluetooth/ReconnectPolicy.h>

#include <auralis/core/LoggingCategories.h>

namespace auralis::bluetooth {

DeviceLifecycleManager::DeviceLifecycleManager(
    IBlueZClient* client,
    DeviceRegistry* registry,
    AdapterManager* adapters,
    BlueZAgent* agent,
    ReconnectPolicy* reconnect,
    QObject* parent)
    : QObject(parent)
    , client_(client)
    , registry_(registry)
    , adapters_(adapters)
    , agent_(agent)
    , reconnect_(reconnect)
{
    if (client_ == nullptr) {
        return;
    }
    connect(client_, &IBlueZClient::pairDeviceFinished, this, &DeviceLifecycleManager::handlePairFinished);
    connect(client_, &IBlueZClient::cancelPairingFinished, this, &DeviceLifecycleManager::handleCancelPairingFinished);
    connect(client_, &IBlueZClient::connectDeviceFinished, this, &DeviceLifecycleManager::handleConnectFinished);
    connect(client_, &IBlueZClient::disconnectDeviceFinished, this, &DeviceLifecycleManager::handleDisconnectFinished);
    connect(client_, &IBlueZClient::setDeviceTrustedFinished, this, &DeviceLifecycleManager::handleTrustFinished);
    connect(client_, &IBlueZClient::removeDeviceFinished, this, &DeviceLifecycleManager::handleRemoveFinished);
    if (reconnect_ != nullptr) {
        connect(reconnect_, &ReconnectPolicy::reconnectDue, this, [this](const QString& devicePath, int attempt, int) {
            registry_->setReconnectAttempt(devicePath, attempt);
            registry_->setOperation(devicePath, DeviceOperation::Reconnecting);
            emit deviceOperationChanged(devicePath);
            client_->connectDevice(devicePath);
        });
    }
}

void DeviceLifecycleManager::shutdown()
{
    pending_.clear();
    if (reconnect_ != nullptr) {
        reconnect_->cancelAll();
    }
}

void DeviceLifecycleManager::onBlueZAvailabilityChanged(bool available)
{
    if (!available) {
        pending_.clear();
        if (reconnect_ != nullptr) {
            reconnect_->pauseAll();
        }
    }
}

void DeviceLifecycleManager::onDeviceRemoved(const QString& objectPath)
{
    clearPending(objectPath);
    if (reconnect_ != nullptr) {
        reconnect_->cancelReconnect(objectPath);
    }
}

void DeviceLifecycleManager::onDevicePropertiesChanged(
    const QString& objectPath,
    const QVariantMap& changed,
    const QStringList&)
{
    const BluetoothDeviceData* device = registry_->findByObjectPath(objectPath);
    if (device == nullptr) {
        return;
    }
    checkPropertyCompletion(objectPath, *device);

    if (changed.contains(bluez::kPropConnected.toString()) && !device->connected && device->paired
        && device->operation == DeviceOperation::Idle) {
        handleUnexpectedDisconnect(objectPath, *device);
    }
    if (changed.contains(bluez::kPropConnected.toString()) && device->connected) {
        registry_->setUserDisconnectRequested(objectPath, false);
        registry_->clearLastError(objectPath);
        if (reconnect_ != nullptr) {
            reconnect_->onConnected(objectPath);
        }
    }
}

void DeviceLifecycleManager::pairDevice(const QString& objectPath)
{
    const BluetoothDeviceData* device = requireDevice(objectPath);
    if (device == nullptr || !canPair(*device) || client_ == nullptr) {
        return;
    }
    if (!beginOperation(objectPath, DeviceOperation::Pairing)) {
        return;
    }
    qCInfo(auralisBluetooth) << "PairRequested" << objectPath;
    client_->pairDevice(objectPath);
}

void DeviceLifecycleManager::cancelPairing(const QString& objectPath)
{
    const BluetoothDeviceData* device = requireDevice(objectPath);
    if (device == nullptr || !canCancelPairing(*device) || client_ == nullptr) {
        return;
    }
    if (!beginOperation(objectPath, DeviceOperation::CancellingPairing)) {
        return;
    }
    qCInfo(auralisBluetooth) << "CancelPairingRequested" << objectPath;
    client_->cancelPairing(objectPath);
}

void DeviceLifecycleManager::trustDevice(const QString& objectPath)
{
    const BluetoothDeviceData* device = requireDevice(objectPath);
    if (device == nullptr || !canTrust(*device) || client_ == nullptr) {
        return;
    }
    if (!beginOperation(objectPath, DeviceOperation::Trusting)) {
        return;
    }
    PendingOp op = pending_.value(objectPath);
    op.expectedTrusted = true;
    op.waitingProperty = true;
    pending_.insert(objectPath, op);
    qCInfo(auralisBluetooth) << "TrustRequested" << objectPath;
    client_->setDeviceTrusted(objectPath, true);
}

void DeviceLifecycleManager::untrustDevice(const QString& objectPath)
{
    const BluetoothDeviceData* device = requireDevice(objectPath);
    if (device == nullptr || !canUntrust(*device) || client_ == nullptr) {
        return;
    }
    if (!beginOperation(objectPath, DeviceOperation::Untrusting)) {
        return;
    }
    PendingOp op = pending_.value(objectPath);
    op.expectedTrusted = false;
    op.waitingProperty = true;
    pending_.insert(objectPath, op);
    qCInfo(auralisBluetooth) << "UntrustRequested" << objectPath;
    client_->setDeviceTrusted(objectPath, false);
}

void DeviceLifecycleManager::connectDevice(const QString& objectPath)
{
    const BluetoothDeviceData* device = requireDevice(objectPath);
    if (device == nullptr || !canConnect(*device) || client_ == nullptr) {
        return;
    }
    if (!beginOperation(objectPath, DeviceOperation::Connecting)) {
        return;
    }
    qCInfo(auralisBluetooth) << "ConnectRequested" << objectPath;
    client_->connectDevice(objectPath);
}

void DeviceLifecycleManager::disconnectDevice(const QString& objectPath)
{
    const BluetoothDeviceData* device = requireDevice(objectPath);
    if (device == nullptr || !canDisconnect(*device) || client_ == nullptr) {
        return;
    }
    registry_->setUserDisconnectRequested(objectPath, true);
    if (reconnect_ != nullptr) {
        reconnect_->cancelReconnect(objectPath);
    }
    if (!beginOperation(objectPath, DeviceOperation::Disconnecting)) {
        return;
    }
    qCInfo(auralisBluetooth) << "DisconnectRequested" << objectPath;
    client_->disconnectDevice(objectPath);
}

void DeviceLifecycleManager::forgetDevice(const QString& objectPath)
{
    const BluetoothDeviceData* device = requireDevice(objectPath);
    if (device == nullptr || !canForget(*device) || client_ == nullptr || adapters_ == nullptr) {
        return;
    }
    if (reconnect_ != nullptr) {
        reconnect_->cancelReconnect(objectPath);
    }
    if (!beginOperation(objectPath, DeviceOperation::Forgetting)) {
        return;
    }
    const QString adapterPath = device->adapterPath.isEmpty() ? adapters_->selectedObjectPath() : device->adapterPath;
    qCInfo(auralisBluetooth) << "ForgetRequested" << objectPath << "adapter=" << adapterPath;
    client_->removeDevice(adapterPath, objectPath);
}

void DeviceLifecycleManager::reconnectDevice(const QString& objectPath)
{
    const BluetoothDeviceData* device = requireDevice(objectPath);
    if (device == nullptr || !canReconnect(*device) || client_ == nullptr) {
        return;
    }
    registry_->setUserDisconnectRequested(objectPath, false);
    if (!beginOperation(objectPath, DeviceOperation::Reconnecting)) {
        return;
    }
    qCInfo(auralisBluetooth) << "ReconnectRequested" << objectPath;
    client_->connectDevice(objectPath);
}

bool DeviceLifecycleManager::beginOperation(const QString& objectPath, DeviceOperation operation)
{
    if (pending_.contains(objectPath)) {
        return false;
    }
    bumpGeneration(objectPath);
    PendingOp op;
    op.operation = operation;
    op.generation = generations_.value(objectPath);
    pending_.insert(objectPath, op);
    registry_->setOperation(objectPath, operation);
    registry_->clearLastError(objectPath);
    emit deviceOperationChanged(objectPath);
    return true;
}

void DeviceLifecycleManager::finishOperation(
    const QString& objectPath,
    quint64 generation,
    bool success,
    BluetoothError error,
    const QString& errorName,
    const QString& message)
{
    if (isStale(objectPath, generation)) {
        return;
    }
    clearPending(objectPath);
    if (!success && error != BluetoothError::None) {
        registry_->setLastError(objectPath, error, errorName, message);
    } else {
        registry_->clearLastError(objectPath);
    }
    registry_->clearOperation(objectPath);
    emit deviceOperationChanged(objectPath);
}

void DeviceLifecycleManager::clearPending(const QString& objectPath)
{
    pending_.remove(objectPath);
    if (registry_ != nullptr && registry_->findByObjectPath(objectPath) != nullptr) {
        registry_->clearOperation(objectPath);
    }
}

quint64 DeviceLifecycleManager::bumpGeneration(const QString& objectPath)
{
    const quint64 next = generations_.value(objectPath) + 1;
    generations_.insert(objectPath, next);
    return next;
}

bool DeviceLifecycleManager::isStale(const QString& objectPath, quint64 generation) const
{
    return generations_.value(objectPath) != generation;
}

const BluetoothDeviceData* DeviceLifecycleManager::requireDevice(const QString& objectPath) const
{
    if (registry_ == nullptr || client_ == nullptr || !client_->isBlueZAvailable()) {
        return nullptr;
    }
    return registry_->findByObjectPath(objectPath);
}

void DeviceLifecycleManager::handleUnexpectedDisconnect(const QString& objectPath, const BluetoothDeviceData& device)
{
    if (device.userDisconnectRequested || !device.autoReconnectEnabled || reconnect_ == nullptr) {
        return;
    }
    reconnect_->scheduleReconnect(objectPath);
}

void DeviceLifecycleManager::handlePairFinished(
    const QString& devicePath,
    bool succeeded,
    const QString& errorName,
    const QString& errorMessage)
{
    const PendingOp op = pending_.value(devicePath);
    if (op.operation != DeviceOperation::Pairing) {
        return;
    }
    if (!succeeded) {
        const auto mapped = mapDbusError(errorName, errorMessage);
        finishOperation(devicePath, op.generation, false, mapped.category, errorName, errorMessage);
        return;
    }
    PendingOp waiting = op;
    waiting.waitingProperty = true;
    pending_.insert(devicePath, waiting);
    const BluetoothDeviceData* device = registry_->findByObjectPath(devicePath);
    if (device != nullptr) {
        checkPropertyCompletion(devicePath, *device);
    }
}

void DeviceLifecycleManager::handleCancelPairingFinished(
    const QString& devicePath,
    bool succeeded,
    const QString& errorName,
    const QString& errorMessage)
{
    const PendingOp op = pending_.value(devicePath);
    if (op.operation != DeviceOperation::CancellingPairing && op.operation != DeviceOperation::Pairing) {
        return;
    }
    if (!succeeded) {
        const auto mapped = mapDbusError(errorName, errorMessage);
        finishOperation(devicePath, op.generation, false, mapped.category, errorName, errorMessage);
        return;
    }
    finishOperation(devicePath, op.generation, true, BluetoothError::None, {}, {});
}

void DeviceLifecycleManager::handleConnectFinished(
    const QString& devicePath,
    bool succeeded,
    const QString& errorName,
    const QString& errorMessage)
{
    const PendingOp op = pending_.value(devicePath);
    if (op.operation != DeviceOperation::Connecting && op.operation != DeviceOperation::Reconnecting) {
        return;
    }
    if (!succeeded) {
        const auto mapped = mapDbusError(errorName, errorMessage);
        if (mapped.category == BluetoothError::AlreadyConnected) {
            finishOperation(devicePath, op.generation, true, BluetoothError::None, {}, {});
            return;
        }
        if (mapped.category == BluetoothError::InProgress) {
            return;
        }
        finishOperation(devicePath, op.generation, false, mapped.category, errorName, errorMessage);
        return;
    }
    PendingOp waiting = op;
    waiting.waitingProperty = true;
    pending_.insert(devicePath, waiting);
    const BluetoothDeviceData* device = registry_->findByObjectPath(devicePath);
    if (device != nullptr) {
        checkPropertyCompletion(devicePath, *device);
    }
}

void DeviceLifecycleManager::handleDisconnectFinished(
    const QString& devicePath,
    bool succeeded,
    const QString& errorName,
    const QString& errorMessage)
{
    const PendingOp op = pending_.value(devicePath);
    if (op.operation != DeviceOperation::Disconnecting) {
        return;
    }
    if (!succeeded) {
        const auto mapped = mapDbusError(errorName, errorMessage);
        if (mapped.category == BluetoothError::NotConnected) {
            finishOperation(devicePath, op.generation, true, BluetoothError::None, {}, {});
            return;
        }
        finishOperation(devicePath, op.generation, false, mapped.category, errorName, errorMessage);
        return;
    }
    PendingOp waiting = op;
    waiting.waitingProperty = true;
    pending_.insert(devicePath, waiting);
    const BluetoothDeviceData* device = registry_->findByObjectPath(devicePath);
    if (device != nullptr) {
        checkPropertyCompletion(devicePath, *device);
    }
}

void DeviceLifecycleManager::handleTrustFinished(
    const QString& devicePath,
    bool trusted,
    bool succeeded,
    const QString& errorName,
    const QString& errorMessage)
{
    const PendingOp op = pending_.value(devicePath);
    if (op.operation != DeviceOperation::Trusting && op.operation != DeviceOperation::Untrusting) {
        return;
    }
    if (!succeeded) {
        const auto mapped = mapDbusError(errorName, errorMessage);
        finishOperation(devicePath, op.generation, false, mapped.category, errorName, errorMessage);
        return;
    }
    PendingOp waiting = op;
    waiting.waitingProperty = true;
    waiting.expectedTrusted = trusted;
    pending_.insert(devicePath, waiting);
    const BluetoothDeviceData* device = registry_->findByObjectPath(devicePath);
    if (device != nullptr) {
        checkPropertyCompletion(devicePath, *device);
    }
}

void DeviceLifecycleManager::handleRemoveFinished(
    const QString&,
    const QString& devicePath,
    bool succeeded,
    const QString& errorName,
    const QString& errorMessage)
{
    const PendingOp op = pending_.value(devicePath);
    if (op.operation != DeviceOperation::Forgetting) {
        return;
    }
    if (!succeeded && registry_->findByObjectPath(devicePath) != nullptr) {
        const auto mapped = mapDbusError(errorName, errorMessage);
        finishOperation(devicePath, op.generation, false, mapped.category, errorName, errorMessage);
        return;
    }
    clearPending(devicePath);
    emit deviceOperationChanged(devicePath);
}

void DeviceLifecycleManager::checkPropertyCompletion(const QString& objectPath, const BluetoothDeviceData& device)
{
    const PendingOp op = pending_.value(objectPath);
    if (op.operation == DeviceOperation::Idle) {
        return;
    }
    if (op.operation == DeviceOperation::Pairing && device.paired) {
        finishOperation(objectPath, op.generation, true, BluetoothError::None, {}, {});
        return;
    }
    if ((op.operation == DeviceOperation::Connecting || op.operation == DeviceOperation::Reconnecting)
        && device.connected) {
        finishOperation(objectPath, op.generation, true, BluetoothError::None, {}, {});
        return;
    }
    if (op.operation == DeviceOperation::Disconnecting && !device.connected) {
        finishOperation(objectPath, op.generation, true, BluetoothError::None, {}, {});
        return;
    }
    if ((op.operation == DeviceOperation::Trusting || op.operation == DeviceOperation::Untrusting)
        && device.trusted == op.expectedTrusted) {
        finishOperation(objectPath, op.generation, true, BluetoothError::None, {}, {});
    }
}

} // namespace auralis::bluetooth

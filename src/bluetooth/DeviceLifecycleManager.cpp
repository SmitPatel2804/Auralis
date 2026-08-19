#include <auralis/bluetooth/DeviceLifecycleManager.h>

#include <auralis/bluetooth/AdapterManager.h>
#include <auralis/bluetooth/BlueZAgent.h>
#include <auralis/bluetooth/BlueZConstants.h>
#include <auralis/bluetooth/BluetoothDbusError.h>
#include <auralis/bluetooth/DeviceOperation.h>
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
            if (!beginOperation(devicePath, DeviceOperation::Reconnecting)) {
                reconnect_->completeReconnectAttempt(devicePath);
                return;
            }
            qCInfo(auralisBluetooth) << "AutoReconnectRequested" << devicePath << "attempt=" << attempt;
            client_->connectDevice(devicePath);
        });
        connect(reconnect_, &ReconnectPolicy::reconnectExhausted, this, [this](const QString& devicePath, int attempts, const QString& reason) {
            qCInfo(auralisBluetooth) << "ManagedReconnectExhausted" << devicePath << "attempts=" << attempts << reason;
            if (registry_ != nullptr) {
                registry_->setReconnectAttempt(devicePath, attempts);
            }
        });
    }
}

void DeviceLifecycleManager::shutdown()
{
    const QStringList activePaths = pending_.keys();
    for (const QString& path : activePaths) {
        stopOperationTimeout(path);
    }
    pending_.clear();
    if (reconnect_ != nullptr) {
        reconnect_->cancelAll();
    }
}

void DeviceLifecycleManager::onBlueZAvailabilityChanged(bool available)
{
    if (!available) {
        const QStringList activePaths = pending_.keys();
        for (const QString& path : activePaths) {
            abortPendingOperation(path, QStringLiteral("BlueZ became unavailable"), BluetoothError::BlueZUnavailable);
        }
        if (reconnect_ != nullptr) {
            reconnect_->pauseAll();
        }
    }
}

void DeviceLifecycleManager::onSnapshotApplied()
{
    if (reconnect_ == nullptr || client_ == nullptr || !client_->isBlueZAvailable()) {
        return;
    }

    reconnect_->resumeAll();
    reevaluateReconnectCandidates();
}

void DeviceLifecycleManager::applyStoredMetadataToRegistry()
{
    if (registry_ == nullptr) {
        return;
    }

    const QVector<BluetoothDeviceData> devices = registry_->devices();
    for (const BluetoothDeviceData& device : devices) {
        const QString key = device.addressIndexKey();
        if (key.isEmpty() || !reconnectMetadata_.contains(key)) {
            continue;
        }
        const DeviceReconnectMetadata metadata = reconnectMetadata_.value(key);
        registry_->setUserDisconnectRequested(device.objectPath, metadata.userDisconnectRequested);
        registry_->setAutoReconnectEnabled(device.objectPath, metadata.autoReconnectEnabled);
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

    device = registry_->findByObjectPath(objectPath);
    if (device == nullptr) {
        return;
    }

    if (changed.contains(bluez::kPropConnected.toString()) && !device->connected && device->paired
        && device->operation == DeviceOperation::Idle) {
        handleUnexpectedDisconnect(objectPath, *device);
    }
    if (changed.contains(bluez::kPropConnected.toString()) && device->connected) {
        clearUserDisconnectSuppression(objectPath);
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
    if (!transitionOperation(objectPath, DeviceOperation::Pairing, DeviceOperation::CancellingPairing)) {
        return;
    }
    if (agent_ != nullptr) {
        agent_->invalidateRequestsForDevice(objectPath, QStringLiteral("Pairing canceled"));
    }
    qCInfo(auralisBluetooth) << "PairingCancelTransitioned" << objectPath;
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
    clearUserDisconnectSuppression(objectPath);
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
    setUserDisconnectRequested(objectPath, true);
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
    removeReconnectMetadata(*device);
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
    clearUserDisconnectSuppression(objectPath);
    if (reconnect_ != nullptr) {
        reconnect_->cancelReconnect(objectPath);
    }
    if (!beginOperation(objectPath, DeviceOperation::Reconnecting)) {
        return;
    }
    qCInfo(auralisBluetooth) << "ReconnectRequested" << objectPath;
    client_->connectDevice(objectPath);
}

void DeviceLifecycleManager::cancelDeviceOperation(const QString& objectPath)
{
    const BluetoothDeviceData* device = requireDevice(objectPath);
    if (device == nullptr) {
        return;
    }
    if (device->operation == DeviceOperation::Pairing) {
        cancelPairing(objectPath);
        return;
    }
    if (!canCancelOperation(*device)) {
        return;
    }
    qCInfo(auralisBluetooth) << "OperationCancelRequested" << objectPath << toString(device->operation);
    abortPendingOperation(objectPath, QStringLiteral("Operation cancelled by user"));
}

int DeviceLifecycleManager::operationTimeoutMs(DeviceOperation operation) const
{
    switch (operation) {
    case DeviceOperation::Connecting:
    case DeviceOperation::Reconnecting:
        return 60000;
    case DeviceOperation::Pairing:
    case DeviceOperation::CancellingPairing:
        return 120000;
    default:
        return 45000;
    }
}

void DeviceLifecycleManager::startOperationTimeout(const QString& objectPath, DeviceOperation operation)
{
    const int timeoutMs = operationTimeoutMs(operation);
    if (timeoutMs <= 0) {
        return;
    }
    PendingOp op = pending_.value(objectPath);
    if (op.timeoutTimer == nullptr) {
        op.timeoutTimer = new QTimer(this);
        op.timeoutTimer->setSingleShot(true);
        connect(op.timeoutTimer, &QTimer::timeout, this, [this, objectPath]() {
            qCWarning(auralisBluetooth) << "OperationTimedOut" << objectPath;
            abortPendingOperation(objectPath, QStringLiteral("Operation timed out"), BluetoothError::TimedOut);
        });
    }
    op.timeoutTimer->start(timeoutMs);
    pending_.insert(objectPath, op);
}

void DeviceLifecycleManager::stopOperationTimeout(const QString& objectPath)
{
    PendingOp op = pending_.value(objectPath);
    if (op.timeoutTimer == nullptr) {
        return;
    }
    op.timeoutTimer->stop();
    op.timeoutTimer->deleteLater();
    op.timeoutTimer = nullptr;
    pending_.insert(objectPath, op);
}

void DeviceLifecycleManager::abortPendingOperation(
    const QString& objectPath,
    const QString& reason,
    BluetoothError error)
{
    PendingOp op = pending_.value(objectPath);
    DeviceOperation activeOp = op.operation;
    const BluetoothDeviceData* device = registry_->findByObjectPath(objectPath);
    if (activeOp == DeviceOperation::Idle && device != nullptr) {
        activeOp = device->operation;
    }

    bumpGeneration(objectPath);
    stopOperationTimeout(objectPath);

    if (activeOp == DeviceOperation::Connecting || activeOp == DeviceOperation::Reconnecting) {
        if (reconnect_ != nullptr) {
            reconnect_->cancelReconnect(objectPath);
        }
        registry_->setUserDisconnectRequested(objectPath, true);
        const BluetoothDeviceData* updated = registry_->findByObjectPath(objectPath);
        if (updated != nullptr) {
            syncReconnectMetadataFromDevice(*updated);
        }
        registry_->setReconnectAttempt(objectPath, 0);
        if (client_ != nullptr && client_->isBlueZAvailable()) {
            client_->disconnectDevice(objectPath);
        }
    }

    pending_.remove(objectPath);
    if (device != nullptr) {
        registry_->clearOperation(objectPath);
        if (error != BluetoothError::None) {
            registry_->setLastError(objectPath, error, {}, reason);
        }
    }
    emit deviceOperationChanged(objectPath);
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
    startOperationTimeout(objectPath, operation);
    emit deviceOperationChanged(objectPath);
    return true;
}

bool DeviceLifecycleManager::transitionOperation(
    const QString& objectPath,
    DeviceOperation from,
    DeviceOperation to)
{
    auto it = pending_.find(objectPath);
    if (it == pending_.end()) {
        return false;
    }
    if (it->operation == to) {
        return true;
    }
    if (it->operation != from) {
        return false;
    }

    const quint64 generation = bumpGeneration(objectPath);
    PendingOp next = it.value();
    next.operation = to;
    next.generation = generation;
    next.waitingProperty = false;
    next.expectedTrusted = false;
    pending_.insert(objectPath, next);
    registry_->setOperation(objectPath, to);
    registry_->clearLastError(objectPath);
    startOperationTimeout(objectPath, to);
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
    stopOperationTimeout(objectPath);
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
    stopOperationTimeout(objectPath);
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
    if (reconnect_->isScheduled(objectPath) || reconnect_->isReconnectInProgress(objectPath)) {
        qCDebug(auralisBluetooth) << "ManagedReconnectAlreadyScheduled" << objectPath;
        return;
    }
    qCInfo(auralisBluetooth) << "ReconnectScheduled" << objectPath << "attempt=" << reconnect_->attempt(objectPath) + 1;
    reconnect_->scheduleReconnect(objectPath);
}

void DeviceLifecycleManager::reevaluateReconnectCandidates()
{
    if (registry_ == nullptr || reconnect_ == nullptr) {
        return;
    }

    const QVector<BluetoothDeviceData> devices = registry_->devices();
    for (const BluetoothDeviceData& device : devices) {
        if (!device.paired || device.connected || !device.autoReconnectEnabled || device.userDisconnectRequested) {
            continue;
        }
        if (device.operation != DeviceOperation::Idle || reconnect_->isScheduled(device.objectPath)
            || reconnect_->isReconnectInProgress(device.objectPath)) {
            continue;
        }
        qCInfo(auralisBluetooth) << "ReconnectResumed" << device.objectPath
                                 << "attempt=" << reconnect_->attempt(device.objectPath) + 1;
        reconnect_->scheduleReconnect(device.objectPath);
    }
}

void DeviceLifecycleManager::syncReconnectMetadataFromDevice(const BluetoothDeviceData& device)
{
    const QString key = device.addressIndexKey();
    if (key.isEmpty()) {
        return;
    }
    reconnectMetadata_.insert(
        key,
        DeviceReconnectMetadata{device.userDisconnectRequested, device.autoReconnectEnabled});
}

void DeviceLifecycleManager::removeReconnectMetadata(const BluetoothDeviceData& device)
{
    const QString key = device.addressIndexKey();
    if (!key.isEmpty()) {
        reconnectMetadata_.remove(key);
    }
}

void DeviceLifecycleManager::setUserDisconnectRequested(const QString& objectPath, bool requested)
{
    if (registry_ == nullptr) {
        return;
    }
    registry_->setUserDisconnectRequested(objectPath, requested);
    const BluetoothDeviceData* device = registry_->findByObjectPath(objectPath);
    if (device != nullptr) {
        syncReconnectMetadataFromDevice(*device);
    }
}

void DeviceLifecycleManager::clearUserDisconnectSuppression(const QString& objectPath)
{
    setUserDisconnectRequested(objectPath, false);
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
        if (op.operation == DeviceOperation::Reconnecting && reconnect_ != nullptr) {
            reconnect_->completeReconnectAttempt(devicePath);
            if (mapped.retryable) {
                qCWarning(auralisBluetooth) << "ReconnectFailedRetryable" << devicePath << errorName << errorMessage;
                finishOperation(devicePath, op.generation, false, mapped.category, errorName, errorMessage);
                if (client_ != nullptr && client_->isBlueZAvailable()) {
                    reconnect_->scheduleReconnect(devicePath);
                }
                return;
            }
            qCWarning(auralisBluetooth) << "ReconnectFailedTerminal" << devicePath << errorName << errorMessage;
            reconnect_->cancelReconnect(devicePath);
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

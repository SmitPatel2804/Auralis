#include <auralis/bluetooth/DeviceOperation.h>

#include <auralis/bluetooth/BlueZTypes.h>

namespace auralis::bluetooth {

QString toString(DeviceOperation operation)
{
    switch (operation) {
    case DeviceOperation::Idle:
        return QStringLiteral("Idle");
    case DeviceOperation::Pairing:
        return QStringLiteral("Pairing");
    case DeviceOperation::CancellingPairing:
        return QStringLiteral("CancellingPairing");
    case DeviceOperation::Trusting:
        return QStringLiteral("Trusting");
    case DeviceOperation::Untrusting:
        return QStringLiteral("Untrusting");
    case DeviceOperation::Connecting:
        return QStringLiteral("Connecting");
    case DeviceOperation::Disconnecting:
        return QStringLiteral("Disconnecting");
    case DeviceOperation::Forgetting:
        return QStringLiteral("Forgetting");
    case DeviceOperation::Reconnecting:
        return QStringLiteral("Reconnecting");
    }
    return QStringLiteral("Unknown");
}

QString toString(DeviceLogicalState state)
{
    switch (state) {
    case DeviceLogicalState::Removed:
        return QStringLiteral("Removed");
    case DeviceLogicalState::Discovered:
        return QStringLiteral("Discovered");
    case DeviceLogicalState::Pairing:
        return QStringLiteral("Pairing");
    case DeviceLogicalState::Paired:
        return QStringLiteral("Paired");
    case DeviceLogicalState::Trusted:
        return QStringLiteral("Trusted");
    case DeviceLogicalState::Connecting:
        return QStringLiteral("Connecting");
    case DeviceLogicalState::Connected:
        return QStringLiteral("Connected");
    case DeviceLogicalState::Disconnecting:
        return QStringLiteral("Disconnecting");
    case DeviceLogicalState::Disconnected:
        return QStringLiteral("Disconnected");
    case DeviceLogicalState::Reconnecting:
        return QStringLiteral("Reconnecting");
    case DeviceLogicalState::Forgetting:
        return QStringLiteral("Forgetting");
    case DeviceLogicalState::Failed:
        return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

QString operationStatusText(DeviceOperation operation, int reconnectAttempt, int maxReconnectAttempts)
{
    switch (operation) {
    case DeviceOperation::Pairing:
        return QStringLiteral("Pairing...");
    case DeviceOperation::CancellingPairing:
        return QStringLiteral("Cancelling pairing...");
    case DeviceOperation::Trusting:
        return QStringLiteral("Trusting...");
    case DeviceOperation::Untrusting:
        return QStringLiteral("Untrusting...");
    case DeviceOperation::Connecting:
        return QStringLiteral("Connecting...");
    case DeviceOperation::Disconnecting:
        return QStringLiteral("Disconnecting...");
    case DeviceOperation::Forgetting:
        return QStringLiteral("Forgetting...");
    case DeviceOperation::Reconnecting:
        if (maxReconnectAttempts > 0) {
            return QStringLiteral("Reconnecting (attempt %1/%2)...")
                .arg(reconnectAttempt)
                .arg(maxReconnectAttempts);
        }
        return QStringLiteral("Reconnecting...");
    case DeviceOperation::Idle:
        return {};
    }
    return {};
}

DeviceLogicalState deriveLogicalState(const BluetoothDeviceData& device)
{
    if (device.objectPath.isEmpty()) {
        return DeviceLogicalState::Removed;
    }
    switch (device.operation) {
    case DeviceOperation::Pairing:
    case DeviceOperation::CancellingPairing:
        return DeviceLogicalState::Pairing;
    case DeviceOperation::Connecting:
        return DeviceLogicalState::Connecting;
    case DeviceOperation::Disconnecting:
        return DeviceLogicalState::Disconnecting;
    case DeviceOperation::Forgetting:
        return DeviceLogicalState::Forgetting;
    case DeviceOperation::Reconnecting:
        return DeviceLogicalState::Reconnecting;
    case DeviceOperation::Idle:
    case DeviceOperation::Trusting:
    case DeviceOperation::Untrusting:
        break;
    }
    if (device.connected) {
        return DeviceLogicalState::Connected;
    }
    if (device.trusted) {
        return DeviceLogicalState::Trusted;
    }
    if (device.paired) {
        return DeviceLogicalState::Paired;
    }
    if (!device.lastErrorMessage.isEmpty() && device.lastError != BluetoothError::None) {
        return DeviceLogicalState::Failed;
    }
    return DeviceLogicalState::Discovered;
}

bool isOperationBusy(DeviceOperation operation)
{
    return operation != DeviceOperation::Idle;
}

bool canPair(const BluetoothDeviceData& device)
{
    return !device.objectPath.isEmpty() && !device.paired && !isOperationBusy(device.operation);
}

bool canCancelPairing(const BluetoothDeviceData& device)
{
    return device.operation == DeviceOperation::Pairing;
}

bool canTrust(const BluetoothDeviceData& device)
{
    return device.paired && !device.trusted && !isOperationBusy(device.operation);
}

bool canUntrust(const BluetoothDeviceData& device)
{
    return device.trusted && !isOperationBusy(device.operation);
}

bool canConnect(const BluetoothDeviceData& device)
{
    return device.paired && !device.connected && !isOperationBusy(device.operation);
}

bool canDisconnect(const BluetoothDeviceData& device)
{
    return device.connected && !isOperationBusy(device.operation);
}

bool canForget(const BluetoothDeviceData& device)
{
    return !device.objectPath.isEmpty() && !isOperationBusy(device.operation);
}

bool canReconnect(const BluetoothDeviceData& device)
{
    return device.paired && !device.connected && !isOperationBusy(device.operation);
}

} // namespace auralis::bluetooth

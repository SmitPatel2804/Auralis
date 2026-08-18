#include <auralis/bluetooth/BluetoothError.h>

namespace auralis::bluetooth {

QString toString(BluetoothError error)
{
    switch (error) {
    case BluetoothError::None:
        return QStringLiteral("None");
    case BluetoothError::NoSystemBus:
        return QStringLiteral("NoSystemBus");
    case BluetoothError::BlueZUnavailable:
        return QStringLiteral("BlueZUnavailable");
    case BluetoothError::NoAdapter:
        return QStringLiteral("NoAdapter");
    case BluetoothError::AdapterPoweredOff:
        return QStringLiteral("AdapterPoweredOff");
    case BluetoothError::DiscoveryStartFailed:
        return QStringLiteral("DiscoveryStartFailed");
    case BluetoothError::DiscoveryStopFailed:
        return QStringLiteral("DiscoveryStopFailed");
    case BluetoothError::DbusCallFailed:
        return QStringLiteral("DbusCallFailed");
    case BluetoothError::MalformedDbusPayload:
        return QStringLiteral("MalformedDbusPayload");
    case BluetoothError::AdapterRemoved:
        return QStringLiteral("AdapterRemoved");
    case BluetoothError::DeviceUnavailable:
        return QStringLiteral("DeviceUnavailable");
    case BluetoothError::NotReady:
        return QStringLiteral("NotReady");
    case BluetoothError::InProgress:
        return QStringLiteral("InProgress");
    case BluetoothError::AlreadyConnected:
        return QStringLiteral("AlreadyConnected");
    case BluetoothError::NotConnected:
        return QStringLiteral("NotConnected");
    case BluetoothError::AuthenticationCanceled:
        return QStringLiteral("AuthenticationCanceled");
    case BluetoothError::AuthenticationFailed:
        return QStringLiteral("AuthenticationFailed");
    case BluetoothError::AuthenticationRejected:
        return QStringLiteral("AuthenticationRejected");
    case BluetoothError::AuthenticationTimeout:
        return QStringLiteral("AuthenticationTimeout");
    case BluetoothError::ConnectionFailed:
        return QStringLiteral("ConnectionFailed");
    case BluetoothError::NotSupported:
        return QStringLiteral("NotSupported");
    case BluetoothError::InvalidArguments:
        return QStringLiteral("InvalidArguments");
    case BluetoothError::TimedOut:
        return QStringLiteral("TimedOut");
    case BluetoothError::PermissionDenied:
        return QStringLiteral("PermissionDenied");
    case BluetoothError::OperationFailed:
        return QStringLiteral("OperationFailed");
    case BluetoothError::AgentNotRegistered:
        return QStringLiteral("AgentNotRegistered");
    case BluetoothError::OperationInProgress:
        return QStringLiteral("OperationInProgress");
    }
    return QStringLiteral("Unknown");
}

QString defaultMessage(BluetoothError error)
{
    switch (error) {
    case BluetoothError::None:
        return {};
    case BluetoothError::NoSystemBus:
        return QStringLiteral("System D-Bus is unavailable");
    case BluetoothError::BlueZUnavailable:
        return QStringLiteral("BlueZ is unavailable");
    case BluetoothError::NoAdapter:
        return QStringLiteral("Bluetooth adapter not available");
    case BluetoothError::AdapterPoweredOff:
        return QStringLiteral("Bluetooth is powered off");
    case BluetoothError::DiscoveryStartFailed:
        return QStringLiteral("Failed to start discovery");
    case BluetoothError::DiscoveryStopFailed:
        return QStringLiteral("Failed to stop discovery");
    case BluetoothError::DbusCallFailed:
        return QStringLiteral("A BlueZ D-Bus call failed");
    case BluetoothError::MalformedDbusPayload:
        return QStringLiteral("Ignored a malformed BlueZ payload");
    case BluetoothError::AdapterRemoved:
        return QStringLiteral("Bluetooth adapter was removed");
    case BluetoothError::DeviceUnavailable:
        return QStringLiteral("Bluetooth device is no longer available");
    case BluetoothError::NotReady:
        return QStringLiteral("Bluetooth is not ready");
    case BluetoothError::InProgress:
        return QStringLiteral("Bluetooth operation already in progress");
    case BluetoothError::AlreadyConnected:
        return QStringLiteral("Device is already connected");
    case BluetoothError::NotConnected:
        return QStringLiteral("Device is not connected");
    case BluetoothError::AuthenticationCanceled:
        return QStringLiteral("Pairing was canceled");
    case BluetoothError::AuthenticationFailed:
        return QStringLiteral("Pairing authentication failed");
    case BluetoothError::AuthenticationRejected:
        return QStringLiteral("Pairing was rejected");
    case BluetoothError::AuthenticationTimeout:
        return QStringLiteral("Pairing timed out");
    case BluetoothError::ConnectionFailed:
        return QStringLiteral("Connection failed");
    case BluetoothError::NotSupported:
        return QStringLiteral("Operation is not supported");
    case BluetoothError::InvalidArguments:
        return QStringLiteral("Invalid Bluetooth operation arguments");
    case BluetoothError::TimedOut:
        return QStringLiteral("Bluetooth operation timed out");
    case BluetoothError::PermissionDenied:
        return QStringLiteral("Bluetooth permission denied");
    case BluetoothError::OperationFailed:
        return QStringLiteral("Bluetooth operation failed");
    case BluetoothError::AgentNotRegistered:
        return QStringLiteral("Pairing agent is not registered");
    case BluetoothError::OperationInProgress:
        return QStringLiteral("Another device operation is already in progress");
    }
    return QStringLiteral("Bluetooth error");
}

} // namespace auralis::bluetooth

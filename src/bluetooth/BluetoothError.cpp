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
    }
    return QStringLiteral("Bluetooth error");
}

} // namespace auralis::bluetooth

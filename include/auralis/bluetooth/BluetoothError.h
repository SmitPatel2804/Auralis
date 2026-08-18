#pragma once

#include <QString>

namespace auralis::bluetooth {

enum class BluetoothError {
    None,
    NoSystemBus,
    BlueZUnavailable,
    NoAdapter,
    AdapterPoweredOff,
    DiscoveryStartFailed,
    DiscoveryStopFailed,
    DbusCallFailed,
    MalformedDbusPayload,
    AdapterRemoved
};

QString toString(BluetoothError error);
QString defaultMessage(BluetoothError error);

} // namespace auralis::bluetooth

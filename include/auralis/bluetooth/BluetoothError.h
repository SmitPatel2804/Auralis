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
    AdapterRemoved,
    DeviceUnavailable,
    NotReady,
    InProgress,
    AlreadyConnected,
    NotConnected,
    AuthenticationCanceled,
    AuthenticationFailed,
    AuthenticationRejected,
    AuthenticationTimeout,
    ConnectionFailed,
    NotSupported,
    InvalidArguments,
    TimedOut,
    PermissionDenied,
    OperationFailed,
    AgentNotRegistered,
    OperationInProgress
};

QString toString(BluetoothError error);
QString defaultMessage(BluetoothError error);

} // namespace auralis::bluetooth

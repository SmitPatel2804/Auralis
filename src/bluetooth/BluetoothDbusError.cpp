#include <auralis/bluetooth/BluetoothDbusError.h>

#include <auralis/bluetooth/BlueZConstants.h>

namespace auralis::bluetooth {

BluetoothOperationError mapDbusError(const QString& errorName, const QString& errorMessage)
{
    BluetoothOperationError result;
    result.dbusErrorName = errorName;
    result.message = errorMessage;

    if (errorName == bluez::kErrorNotReady.toString()) {
        result.category = BluetoothError::NotReady;
        result.retryable = true;
        return result;
    }
    if (errorName == bluez::kErrorInProgress.toString()) {
        result.category = BluetoothError::InProgress;
        result.retryable = true;
        return result;
    }
    if (errorName == bluez::kErrorNotAuthorized.toString()) {
        result.category = BluetoothError::PermissionDenied;
        return result;
    }
    if (errorName == bluez::kErrorFailed.toString()) {
        result.category = BluetoothError::OperationFailed;
        return result;
    }
    if (errorName == bluez::kErrorAlreadyConnected.toString()) {
        result.category = BluetoothError::AlreadyConnected;
        return result;
    }
    if (errorName == bluez::kErrorNotConnected.toString()) {
        result.category = BluetoothError::NotConnected;
        return result;
    }
    if (errorName == bluez::kErrorAuthenticationCanceled.toString()) {
        result.category = BluetoothError::AuthenticationCanceled;
        return result;
    }
    if (errorName == bluez::kErrorAuthenticationFailed.toString()) {
        result.category = BluetoothError::AuthenticationFailed;
        return result;
    }
    if (errorName == bluez::kErrorAuthenticationRejected.toString()) {
        result.category = BluetoothError::AuthenticationRejected;
        return result;
    }
    if (errorName == bluez::kErrorAuthenticationTimeout.toString()) {
        result.category = BluetoothError::AuthenticationTimeout;
        return result;
    }
    if (errorName == bluez::kErrorConnectionAttemptFailed.toString()) {
        result.category = BluetoothError::ConnectionFailed;
        result.retryable = true;
        return result;
    }
    if (errorName == bluez::kErrorNotSupported.toString()) {
        result.category = BluetoothError::NotSupported;
        return result;
    }
    if (errorName == bluez::kErrorInvalidArguments.toString()) {
        result.category = BluetoothError::InvalidArguments;
        return result;
    }
    if (errorName == QStringLiteral("org.freedesktop.DBus.Error.NoReply")
        || errorName == QStringLiteral("org.freedesktop.DBus.Error.Timeout")) {
        result.category = BluetoothError::TimedOut;
        result.retryable = true;
        return result;
    }
    if (errorName == QStringLiteral("org.freedesktop.DBus.Error.Disconnected")
        || errorName == QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown")) {
        result.category = BluetoothError::BlueZUnavailable;
        return result;
    }

    result.category = BluetoothError::DbusCallFailed;
    return result;
}

} // namespace auralis::bluetooth

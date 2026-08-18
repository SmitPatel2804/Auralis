#pragma once

#include <auralis/bluetooth/BluetoothError.h>

#include <QString>

namespace auralis::bluetooth {

struct BluetoothOperationError {
    BluetoothError category = BluetoothError::None;
    QString dbusErrorName;
    QString message;
    bool retryable = false;
};

BluetoothOperationError mapDbusError(const QString& errorName, const QString& errorMessage);

} // namespace auralis::bluetooth

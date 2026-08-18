#pragma once

#include <QString>

namespace auralis::bluetooth {

struct BluetoothDeviceData;

enum class DeviceOperation {
    Idle,
    Pairing,
    CancellingPairing,
    Trusting,
    Untrusting,
    Connecting,
    Disconnecting,
    Forgetting,
    Reconnecting
};

enum class DeviceLogicalState {
    Removed,
    Discovered,
    Pairing,
    Paired,
    Trusted,
    Connecting,
    Connected,
    Disconnecting,
    Disconnected,
    Reconnecting,
    Forgetting,
    Failed
};

QString toString(DeviceOperation operation);
QString toString(DeviceLogicalState state);
QString operationStatusText(DeviceOperation operation, int reconnectAttempt = 0, int maxReconnectAttempts = 0);

DeviceLogicalState deriveLogicalState(const BluetoothDeviceData& device);

bool isOperationBusy(DeviceOperation operation);
bool canPair(const BluetoothDeviceData& device);
bool canCancelPairing(const BluetoothDeviceData& device);
bool canTrust(const BluetoothDeviceData& device);
bool canUntrust(const BluetoothDeviceData& device);
bool canConnect(const BluetoothDeviceData& device);
bool canDisconnect(const BluetoothDeviceData& device);
bool canForget(const BluetoothDeviceData& device);
bool canReconnect(const BluetoothDeviceData& device);
bool canCancelOperation(const BluetoothDeviceData& device);

} // namespace auralis::bluetooth

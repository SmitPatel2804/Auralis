#pragma once

#include <QString>

namespace auralis::bluetooth {

enum class DeviceButtonPolicy {
    Allow = 0,
    Disallow = 1,
};

enum class DeviceButtonEffectiveState {
    Allowed = 0,
    Suppressed = 1,
    Unsupported = 2,
    PermissionDenied = 3,
};

inline QString deviceButtonPolicyText(DeviceButtonPolicy policy)
{
    return policy == DeviceButtonPolicy::Disallow ? QStringLiteral("DISALLOW") : QStringLiteral("ALLOW");
}

inline QString deviceButtonEffectiveStateText(DeviceButtonEffectiveState state)
{
    switch (state) {
    case DeviceButtonEffectiveState::Suppressed:
        return QStringLiteral("Suppressed");
    case DeviceButtonEffectiveState::Unsupported:
        return QStringLiteral("Unsupported on this transport");
    case DeviceButtonEffectiveState::PermissionDenied:
        return QStringLiteral("Permission denied for /dev/input");
    case DeviceButtonEffectiveState::Allowed:
    default:
        return QStringLiteral("Allowed");
    }
}

struct BluetoothInputEndpoint {
    QString address;
    QString eventNode;
    QString name;
};

} // namespace auralis::bluetooth

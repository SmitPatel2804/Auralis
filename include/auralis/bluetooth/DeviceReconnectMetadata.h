#pragma once

namespace auralis::bluetooth {

struct DeviceReconnectMetadata {
    bool userDisconnectRequested = false;
    bool autoReconnectEnabled = true;
};

} // namespace auralis::bluetooth

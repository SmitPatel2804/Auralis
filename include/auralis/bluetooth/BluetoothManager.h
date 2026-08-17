#pragma once

#include <auralis/bluetooth/IBluetoothManager.h>

namespace auralis::bluetooth {

// Phase 1 foundation stub. Real BlueZ integration begins in Phase 2.
class BluetoothManager final : public IBluetoothManager {
public:
    bool initialize() override;
    void shutdown() override;
    auralis::core::ServiceStatus status() const noexcept override;

private:
    auralis::core::ServiceStatus status_ = auralis::core::ServiceStatus::Uninitialized;
};

} // namespace auralis::bluetooth

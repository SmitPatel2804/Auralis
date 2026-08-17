#pragma once

#include <auralis/devices/IDeviceManager.h>

namespace auralis::devices {

// Phase 1 foundation stub. Higher-level device models begin in later phases.
class DeviceManager final : public IDeviceManager {
public:
    bool initialize() override;
    void shutdown() override;
    auralis::core::ServiceStatus status() const noexcept override;

private:
    auralis::core::ServiceStatus status_ = auralis::core::ServiceStatus::Uninitialized;
};

} // namespace auralis::devices

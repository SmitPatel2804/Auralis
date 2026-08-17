#include <auralis/devices/DeviceManager.h>

#include <auralis/core/LoggingCategories.h>

namespace auralis::devices {

bool DeviceManager::initialize()
{
    // Phase 1 foundation stub. Device discovery and domain models begin in later phases.
    status_ = auralis::core::ServiceStatus::Initializing;
    status_ = auralis::core::ServiceStatus::Ready;
    qCInfo(auralisDevices) << "Device manager skeleton ready";
    return true;
}

void DeviceManager::shutdown()
{
    if (status_ == auralis::core::ServiceStatus::Uninitialized) {
        return;
    }

    qCInfo(auralisDevices) << "Device manager skeleton shut down";
    status_ = auralis::core::ServiceStatus::Uninitialized;
}

auralis::core::ServiceStatus DeviceManager::status() const noexcept
{
    return status_;
}

} // namespace auralis::devices

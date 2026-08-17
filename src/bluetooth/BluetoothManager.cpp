#include <auralis/bluetooth/BluetoothManager.h>

#include <auralis/core/LoggingCategories.h>

namespace auralis::bluetooth {

bool BluetoothManager::initialize()
{
    // Phase 1 foundation stub. Real BlueZ integration begins in Phase 2.
    // Ready means this service object is initialized, not that an adapter was queried.
    status_ = auralis::core::ServiceStatus::Initializing;
    status_ = auralis::core::ServiceStatus::Ready;
    qCInfo(auralisBluetooth) << "Bluetooth manager skeleton ready";
    return true;
}

void BluetoothManager::shutdown()
{
    if (status_ == auralis::core::ServiceStatus::Uninitialized) {
        return;
    }

    qCInfo(auralisBluetooth) << "Bluetooth manager skeleton shut down";
    status_ = auralis::core::ServiceStatus::Uninitialized;
}

auralis::core::ServiceStatus BluetoothManager::status() const noexcept
{
    return status_;
}

} // namespace auralis::bluetooth

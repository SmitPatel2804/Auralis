#pragma once

#include <auralis/core/ServiceStatus.h>

namespace auralis::bluetooth {

class IBluetoothManager {
public:
    virtual ~IBluetoothManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual auralis::core::ServiceStatus status() const noexcept = 0;
};

} // namespace auralis::bluetooth

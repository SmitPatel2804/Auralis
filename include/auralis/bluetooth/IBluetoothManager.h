#pragma once

#include <auralis/core/ServiceStatus.h>

class QObject;

namespace auralis::bluetooth {

class IBluetoothManager {
public:
    virtual ~IBluetoothManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual auralis::core::ServiceStatus status() const noexcept = 0;

    // Phase 2: production BluetoothManager returns itself for QML. Fakes stay nullptr.
    virtual QObject* uiObject() { return nullptr; }
};

} // namespace auralis::bluetooth

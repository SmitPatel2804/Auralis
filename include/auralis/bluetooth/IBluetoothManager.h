#pragma once

#include <auralis/core/ServiceStatus.h>

#include <QString>

class QObject;

namespace auralis::bluetooth {

class DeviceRegistry;

class IBluetoothManager {
public:
    virtual ~IBluetoothManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual auralis::core::ServiceStatus status() const noexcept = 0;

    // Phase 2: production BluetoothManager returns itself for QML. Fakes stay nullptr.
    virtual QObject* uiObject() { return nullptr; }

    virtual QString backendName() const { return QStringLiteral("Unknown"); }
    virtual bool available() const noexcept { return status() == auralis::core::ServiceStatus::Ready; }
    virtual bool transportConnected() const noexcept { return available(); }
    virtual bool adapterPresent() const noexcept { return available(); }
    virtual void refresh() {}
    virtual void pauseManagedReconnect() {}
    virtual void resumeManagedReconnect() {}
    virtual void requestManagedReconnect(const QString&) {}
    virtual void cancelManagedReconnect(const QString&) {}
    virtual void suppressAutoReconnect(const QString&) {}
    virtual void unsuppressAutoReconnect(const QString&) {}
    virtual DeviceRegistry* deviceRegistry() const noexcept { return nullptr; }
};

} // namespace auralis::bluetooth

#pragma once

#include <auralis/core/ServiceStatus.h>

#include <QString>

class QObject;

namespace auralis::audio {

class AudioEndpointRegistry;
class AudioRouter;

/// Platform-neutral audio service contract.
///
/// Linux implements this with PipeWire. Windows and macOS provide native
/// graph/routing implementations while the session engine and QML keep the
/// same behavior and object contract.
class IAudioManager {
public:
    virtual ~IAudioManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual auralis::core::ServiceStatus status() const noexcept = 0;
    virtual QObject* uiObject() { return nullptr; }

    virtual QString backendName() const { return QStringLiteral("Unknown"); }
    virtual QString lastError() const { return {}; }
    virtual bool connected() const noexcept { return status() == auralis::core::ServiceStatus::Ready; }
    virtual bool graphReady() const noexcept { return connected(); }
    virtual void setAutoReconnectEnabled(bool) {}
    virtual void requestReconnect() {}

    virtual AudioRouter* audioRouter() const noexcept { return nullptr; }
    virtual AudioEndpointRegistry* endpointRegistry() const noexcept { return nullptr; }
};

} // namespace auralis::audio

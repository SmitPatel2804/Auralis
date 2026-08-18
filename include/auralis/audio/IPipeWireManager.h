#pragma once

#include <auralis/core/ServiceStatus.h>

class QObject;

namespace auralis::audio {

class IPipeWireManager {
public:
    virtual ~IPipeWireManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual auralis::core::ServiceStatus status() const noexcept = 0;
    virtual QObject* uiObject() { return nullptr; }
};

} // namespace auralis::audio

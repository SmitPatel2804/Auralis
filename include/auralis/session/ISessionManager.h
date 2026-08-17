#pragma once

#include <auralis/core/ServiceStatus.h>

namespace auralis::session {

class ISessionManager {
public:
    virtual ~ISessionManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual auralis::core::ServiceStatus status() const noexcept = 0;
};

} // namespace auralis::session

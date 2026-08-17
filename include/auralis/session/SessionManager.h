#pragma once

#include <auralis/session/ISessionManager.h>

namespace auralis::session {

// Phase 1 foundation stub. Multi-device session behavior begins in Phase 6.
class SessionManager final : public ISessionManager {
public:
    bool initialize() override;
    void shutdown() override;
    auralis::core::ServiceStatus status() const noexcept override;

private:
    auralis::core::ServiceStatus status_ = auralis::core::ServiceStatus::Uninitialized;
};

} // namespace auralis::session

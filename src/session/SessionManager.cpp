#include <auralis/session/SessionManager.h>

#include <auralis/core/LoggingCategories.h>

namespace auralis::session {

bool SessionManager::initialize()
{
    // Phase 1 foundation stub. Multi-device session behavior begins in Phase 6.
    status_ = auralis::core::ServiceStatus::Initializing;
    status_ = auralis::core::ServiceStatus::Ready;
    qCInfo(auralisSession) << "Session manager skeleton ready";
    return true;
}

void SessionManager::shutdown()
{
    if (status_ == auralis::core::ServiceStatus::Uninitialized) {
        return;
    }

    qCInfo(auralisSession) << "Session manager skeleton shut down";
    status_ = auralis::core::ServiceStatus::Uninitialized;
}

auralis::core::ServiceStatus SessionManager::status() const noexcept
{
    return status_;
}

} // namespace auralis::session

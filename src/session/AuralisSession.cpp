#include <auralis/session/AuralisSession.h>

namespace auralis::session {

SessionHealthSnapshot healthSnapshotFromSession(const AuralisSession& session, bool sourceAvailable)
{
    SessionHealthSnapshot health;
    health.sourceAvailable = sourceAvailable && !session.sourceId.isEmpty();
    for (const SessionDevice& device : session.devices) {
        if (!device.enabled) {
            continue;
        }
        ++health.enabledCount;
        if (device.runtime.recovering) {
            ++health.recoveringCount;
        }
        if (device.runtime.routeActive) {
            ++health.routeActiveCount;
        } else if (device.runtime.routeRequested) {
            ++health.pendingCount;
        } else if (device.runtime.lastError.category == SessionError::RouteActivationFailed
            || device.runtime.lastError.category == SessionError::RouteCreationFailed) {
            health.terminalFailure = true;
        }
    }
    if (health.enabledCount == 0) {
        health.terminalFailure = true;
    } else if (session.state == SessionState::Starting) {
        health.terminalFailure = !health.sourceAvailable && health.routeActiveCount == 0
            && health.recoveringCount == 0 && health.pendingCount == 0;
    } else if (!health.sourceAvailable) {
        health.terminalFailure = health.routeActiveCount == 0 && health.recoveringCount == 0
            && health.pendingCount == 0;
    } else if (health.routeActiveCount == 0 && health.recoveringCount == 0 && health.pendingCount == 0) {
        health.terminalFailure = true;
    }
    return health;
}

} // namespace auralis::session

#include <auralis/session/SessionStateMachine.h>

namespace auralis::session {

SessionState SessionStateMachine::recompute(
    SessionState current,
    SessionIntent intent,
    const SessionHealthSnapshot& health)
{
    if (intent == SessionIntent::Stopping || current == SessionState::Stopping) {
        if (health.routeActiveCount == 0 && health.recoveringCount == 0) {
            return SessionState::Idle;
        }
        return SessionState::Stopping;
    }

    if (current == SessionState::Idle && intent == SessionIntent::None) {
        return SessionState::Idle;
    }

    if (intent == SessionIntent::Starting || current == SessionState::Starting) {
        if (health.enabledCount > 0 && health.routeActiveCount >= health.enabledCount && health.sourceAvailable) {
            return SessionState::Active;
        }
        if (health.routeActiveCount > 0) {
            return health.recoveringCount > 0 ? SessionState::Recovering : SessionState::Degraded;
        }
        if (health.pendingCount > 0 || health.recoveringCount > 0) {
            return SessionState::Starting;
        }
        if (health.terminalFailure && health.routeActiveCount == 0) {
            return SessionState::Failed;
        }
        return SessionState::Starting;
    }

    if (intent == SessionIntent::Recovering || current == SessionState::Recovering) {
        if (health.enabledCount > 0 && health.routeActiveCount >= health.enabledCount && health.sourceAvailable) {
            return SessionState::Active;
        }
        if (health.recoveringCount > 0) {
            return SessionState::Recovering;
        }
        if (health.routeActiveCount > 0) {
            return SessionState::Degraded;
        }
        if (health.terminalFailure) {
            return SessionState::Failed;
        }
        return SessionState::Degraded;
    }

    if (current == SessionState::Failed) {
        if (intent == SessionIntent::Starting) {
            return SessionState::Starting;
        }
        return SessionState::Failed;
    }

    if (health.enabledCount == 0) {
        return health.routeActiveCount > 0 ? SessionState::Stopping : SessionState::Failed;
    }

    if (!health.sourceAvailable && health.routeActiveCount == 0) {
        return health.recoveringCount > 0 ? SessionState::Recovering : SessionState::Failed;
    }

    if (health.routeActiveCount >= health.enabledCount && health.sourceAvailable) {
        return SessionState::Active;
    }

    if (health.routeActiveCount > 0) {
        return health.recoveringCount > 0 ? SessionState::Recovering : SessionState::Degraded;
    }

    if (health.recoveringCount > 0) {
        return SessionState::Recovering;
    }

    if (health.terminalFailure) {
        return SessionState::Failed;
    }

    return SessionState::Degraded;
}

} // namespace auralis::session

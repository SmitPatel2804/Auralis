#pragma once

#include <auralis/session/AuralisSession.h>

namespace auralis::session {

class SessionStateMachine {
public:
    [[nodiscard]] static SessionState recompute(
        SessionState current,
        SessionIntent intent,
        const SessionHealthSnapshot& health);
};

} // namespace auralis::session

#include <auralis/session/SessionTypes.h>

namespace auralis::session {

QString toString(SessionState state)
{
    switch (state) {
    case SessionState::Idle:
        return QStringLiteral("Idle");
    case SessionState::Starting:
        return QStringLiteral("Starting");
    case SessionState::Active:
        return QStringLiteral("Active");
    case SessionState::Degraded:
        return QStringLiteral("Degraded");
    case SessionState::Recovering:
        return QStringLiteral("Recovering");
    case SessionState::Stopping:
        return QStringLiteral("Stopping");
    case SessionState::Failed:
        return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

QString toString(SessionDeviceRole role)
{
    switch (role) {
    case SessionDeviceRole::Unspecified:
        return QStringLiteral("Unspecified");
    case SessionDeviceRole::Left:
        return QStringLiteral("Left");
    case SessionDeviceRole::Right:
        return QStringLiteral("Right");
    case SessionDeviceRole::Center:
        return QStringLiteral("Center");
    case SessionDeviceRole::Auxiliary:
        return QStringLiteral("Auxiliary");
    }
    return QStringLiteral("Unspecified");
}

QString toString(RecoveryPolicy policy)
{
    switch (policy) {
    case RecoveryPolicy::None:
        return QStringLiteral("None");
    case RecoveryPolicy::RestoreRoutesOnly:
        return QStringLiteral("RestoreRoutesOnly");
    case RecoveryPolicy::ReconnectAndRestore:
        return QStringLiteral("ReconnectAndRestore");
    }
    return QStringLiteral("None");
}

QString toString(SessionError error)
{
    switch (error) {
    case SessionError::None:
        return QStringLiteral("None");
    case SessionError::SessionNotFound:
        return QStringLiteral("SessionNotFound");
    case SessionError::InvalidSessionName:
        return QStringLiteral("InvalidSessionName");
    case SessionError::NoSourceConfigured:
        return QStringLiteral("NoSourceConfigured");
    case SessionError::NoMembersConfigured:
        return QStringLiteral("NoMembersConfigured");
    case SessionError::DuplicateMember:
        return QStringLiteral("DuplicateMember");
    case SessionError::MemberNotFound:
        return QStringLiteral("MemberNotFound");
    case SessionError::SourceUnavailable:
        return QStringLiteral("SourceUnavailable");
    case SessionError::DeviceUnavailable:
        return QStringLiteral("DeviceUnavailable");
    case SessionError::EndpointUnavailable:
        return QStringLiteral("EndpointUnavailable");
    case SessionError::RouteCreationFailed:
        return QStringLiteral("RouteCreationFailed");
    case SessionError::RouteActivationFailed:
        return QStringLiteral("RouteActivationFailed");
    case SessionError::PersistenceFailure:
        return QStringLiteral("PersistenceFailure");
    case SessionError::OperationAlreadyInProgress:
        return QStringLiteral("OperationAlreadyInProgress");
    case SessionError::InvalidState:
        return QStringLiteral("InvalidState");
    case SessionError::RecoveryExhausted:
        return QStringLiteral("RecoveryExhausted");
    case SessionError::AlreadyActive:
        return QStringLiteral("AlreadyActive");
    case SessionError::InternalError:
        return QStringLiteral("InternalError");
    }
    return QStringLiteral("InternalError");
}

QString toString(SessionCommandResult result)
{
    switch (result) {
    case SessionCommandResult::Accepted:
        return QStringLiteral("Accepted");
    case SessionCommandResult::SessionNotFound:
        return QStringLiteral("SessionNotFound");
    case SessionCommandResult::InvalidSessionName:
        return QStringLiteral("InvalidSessionName");
    case SessionCommandResult::NoSourceConfigured:
        return QStringLiteral("NoSourceConfigured");
    case SessionCommandResult::NoMembersConfigured:
        return QStringLiteral("NoMembersConfigured");
    case SessionCommandResult::DuplicateMember:
        return QStringLiteral("DuplicateMember");
    case SessionCommandResult::MemberNotFound:
        return QStringLiteral("MemberNotFound");
    case SessionCommandResult::AlreadyActive:
        return QStringLiteral("AlreadyActive");
    case SessionCommandResult::InvalidState:
        return QStringLiteral("InvalidState");
    case SessionCommandResult::InvalidArgument:
        return QStringLiteral("InvalidArgument");
    case SessionCommandResult::PersistenceFailure:
        return QStringLiteral("PersistenceFailure");
    case SessionCommandResult::InternalError:
        return QStringLiteral("InternalError");
    }
    return QStringLiteral("InternalError");
}

SessionDeviceRole sessionDeviceRoleFromString(const QString& value, bool* ok)
{
    const QString normalized = value.trimmed();
    if (normalized.compare(QStringLiteral("Left"), Qt::CaseInsensitive) == 0) {
        if (ok != nullptr) {
            *ok = true;
        }
        return SessionDeviceRole::Left;
    }
    if (normalized.compare(QStringLiteral("Right"), Qt::CaseInsensitive) == 0) {
        if (ok != nullptr) {
            *ok = true;
        }
        return SessionDeviceRole::Right;
    }
    if (normalized.compare(QStringLiteral("Center"), Qt::CaseInsensitive) == 0) {
        if (ok != nullptr) {
            *ok = true;
        }
        return SessionDeviceRole::Center;
    }
    if (normalized.compare(QStringLiteral("Auxiliary"), Qt::CaseInsensitive) == 0) {
        if (ok != nullptr) {
            *ok = true;
        }
        return SessionDeviceRole::Auxiliary;
    }
    if (ok != nullptr) {
        *ok = normalized.isEmpty()
            || normalized.compare(QStringLiteral("Unspecified"), Qt::CaseInsensitive) == 0;
    }
    return SessionDeviceRole::Unspecified;
}

RecoveryPolicy recoveryPolicyFromString(const QString& value, bool* ok)
{
    const QString normalized = value.trimmed();
    if (normalized.compare(QStringLiteral("RestoreRoutesOnly"), Qt::CaseInsensitive) == 0
        || normalized.compare(QStringLiteral("restore-routes-only"), Qt::CaseInsensitive) == 0) {
        if (ok != nullptr) {
            *ok = true;
        }
        return RecoveryPolicy::RestoreRoutesOnly;
    }
    if (normalized.compare(QStringLiteral("ReconnectAndRestore"), Qt::CaseInsensitive) == 0
        || normalized.compare(QStringLiteral("reconnect-and-restore"), Qt::CaseInsensitive) == 0) {
        if (ok != nullptr) {
            *ok = true;
        }
        return RecoveryPolicy::ReconnectAndRestore;
    }
    if (ok != nullptr) {
        *ok = normalized.isEmpty() || normalized.compare(QStringLiteral("None"), Qt::CaseInsensitive) == 0;
    }
    return RecoveryPolicy::None;
}

SessionState sessionStateFromString(const QString& value, bool* ok)
{
    const QString normalized = value.trimmed();
    for (int i = 0; i <= static_cast<int>(SessionState::Failed); ++i) {
        const auto state = static_cast<SessionState>(i);
        if (toString(state).compare(normalized, Qt::CaseInsensitive) == 0) {
            if (ok != nullptr) {
                *ok = true;
            }
            return state;
        }
    }
    if (ok != nullptr) {
        *ok = false;
    }
    return SessionState::Idle;
}

} // namespace auralis::session

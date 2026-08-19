#pragma once

#include <QString>

namespace auralis::session {

enum class SessionState {
    Idle,
    Starting,
    Active,
    Degraded,
    Recovering,
    Stopping,
    Failed
};

enum class SessionDeviceRole {
    Unspecified,
    Left,
    Right,
    Center,
    Auxiliary
};

enum class RecoveryPolicy {
    None,
    RestoreRoutesOnly,
    ReconnectAndRestore
};

enum class SessionError {
    None,
    SessionNotFound,
    InvalidSessionName,
    NoSourceConfigured,
    NoMembersConfigured,
    DuplicateMember,
    MemberNotFound,
    SourceUnavailable,
    DeviceUnavailable,
    EndpointUnavailable,
    RouteCreationFailed,
    RouteActivationFailed,
    PersistenceFailure,
    OperationAlreadyInProgress,
    InvalidState,
    RecoveryExhausted,
    AlreadyActive,
    InternalError
};

enum class SessionCommandResult {
    Accepted,
    SessionNotFound,
    InvalidSessionName,
    NoSourceConfigured,
    NoMembersConfigured,
    DuplicateMember,
    MemberNotFound,
    AlreadyActive,
    InvalidState,
    InvalidArgument,
    PersistenceFailure,
    InternalError
};

struct SessionErrorInfo {
    SessionError category = SessionError::None;
    QString detail;

    bool hasError() const noexcept { return category != SessionError::None; }
};

enum class SessionIntent {
    None,
    Starting,
    Stopping,
    Recovering
};

struct MemberHealth {
    bool enabled = false;
    bool connected = false;
    bool endpointAvailable = false;
    bool routeActive = false;
    bool recovering = false;
};

struct SessionHealthSnapshot {
    bool sourceAvailable = false;
    int enabledCount = 0;
    int routeActiveCount = 0;
    int recoveringCount = 0;
    bool terminalFailure = false;
};

QString toString(SessionState state);
QString toString(SessionDeviceRole role);
QString toString(RecoveryPolicy policy);
QString toString(SessionError error);
QString toString(SessionCommandResult result);

SessionDeviceRole sessionDeviceRoleFromString(const QString& value, bool* ok = nullptr);
RecoveryPolicy recoveryPolicyFromString(const QString& value, bool* ok = nullptr);
SessionState sessionStateFromString(const QString& value, bool* ok = nullptr);

} // namespace auralis::session

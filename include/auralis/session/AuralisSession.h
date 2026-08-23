#pragma once

#include <auralis/session/SessionTypes.h>

#include <QDateTime>
#include <QString>
#include <QVector>

namespace auralis::session {

struct SessionMemberRuntime {
    bool connected = false;
    bool endpointAvailable = false;
    bool routeRequested = false;
    bool routeActive = false;
    bool recovering = false;
    /// When false, RoutingCoordinator must not create a new route for this member
    /// (RecoveryPolicy::None after loss, or disabled/removed recovery).
    bool autoRestoreAllowed = true;
    bool managedReconnectRequested = false;
    QString endpointId;
    QString routeId;
    SessionErrorInfo lastError;
};

struct SessionDevice {
    QString deviceId;
    SessionDeviceRole role = SessionDeviceRole::Unspecified;
    bool enabled = true;
    double volumeTrim = 1.0;
    bool muted = false;
    double delayMs = 0.0;
    SessionMemberRuntime runtime;
};

struct AuralisSession {
    QString id;
    QString name;
    QString sourceId;
    QVector<SessionDevice> devices;
    SessionState state = SessionState::Idle;
    SessionErrorInfo error;
    double groupVolume = 1.0;
    bool muted = false;
    bool autoReconnect = true;
    RecoveryPolicy recoveryPolicy = RecoveryPolicy::ReconnectAndRestore;
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime lastUsedAt;
    bool restoreIntent = false;
    quint64 operationGeneration = 0;
};

SessionHealthSnapshot healthSnapshotFromSession(const AuralisSession& session, bool sourceAvailable);

} // namespace auralis::session

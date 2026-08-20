#pragma once

#include <auralis/session/AuralisSession.h>
#include <auralis/session/ISessionManager.h>
#include <auralis/session/RoutingCoordinator.h>
#include <auralis/session/SessionPersistence.h>
#include <auralis/session/SessionTypes.h>
#include <auralis/session/VolumeCoordinator.h>
#include <auralis/audio/AudioRoute.h>

#include <QAbstractItemModel>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

namespace auralis::audio {
class AudioRouter;
class PipeWireManager;
}

namespace auralis::bluetooth {
class BluetoothManager;
class DeviceRegistry;
}

namespace auralis::session {

class SelectedSessionViewModel;
class SessionListModel;
class SessionMemberListModel;

class SessionManager final : public QObject, public ISessionManager {
    Q_OBJECT
    Q_PROPERTY(int sessionCount READ sessionCount NOTIFY sessionsChanged)
    Q_PROPERTY(QString currentSessionId READ currentSessionId NOTIFY currentSessionIdChanged)
    Q_PROPERTY(QString sessionStateText READ sessionStateText NOTIFY sessionStateTextChanged)
    Q_PROPERTY(double groupVolume READ groupVolume NOTIFY groupVolumeChanged)
    Q_PROPERTY(QString currentSourceId READ currentSourceId NOTIFY currentSourceIdChanged)
    Q_PROPERTY(bool currentMuted READ currentMuted NOTIFY currentMutedChanged)
    Q_PROPERTY(QAbstractItemModel* sessionList READ sessionList CONSTANT)
    Q_PROPERTY(QAbstractItemModel* sessionMembers READ sessionMembers CONSTANT)
    Q_PROPERTY(QAbstractItemModel* currentMembers READ currentMembers CONSTANT)
    Q_PROPERTY(QObject* selectedSession READ selectedSession CONSTANT)

public:
    SessionManager(QObject* parent = nullptr);
    SessionManager(
        auralis::bluetooth::BluetoothManager* bluetooth,
        auralis::audio::PipeWireManager* pipeWire,
        const QString& persistencePath,
        QObject* parent = nullptr);
    SessionManager(
        auralis::audio::AudioRouter* router,
        auralis::audio::AudioEndpointRegistry* endpoints,
        auralis::bluetooth::DeviceRegistry* devices,
        auralis::bluetooth::BluetoothManager* bluetooth,
        const QString& persistencePath,
        QObject* parent = nullptr);
    ~SessionManager() override;

    bool initialize() override;
    void shutdown() override;
    auralis::core::ServiceStatus status() const noexcept override;
    QObject* uiObject() override;

    int sessionCount() const;
    QString currentSessionId() const;
    QString sessionStateText() const;
    double groupVolume() const;
    QString currentSourceId() const;
    bool currentMuted() const;
    QAbstractItemModel* sessionList() const;
    QAbstractItemModel* sessionMembers() const;
    QAbstractItemModel* currentMembers() const;
    QObject* selectedSession() const;
    auralis::bluetooth::DeviceRegistry* deviceRegistry() const noexcept;

    QVector<AuralisSession> sessions() const;
    std::optional<AuralisSession> sessionById(const QString& id) const;

    Q_INVOKABLE QString createSession(const QString& name);
    Q_INVOKABLE SessionCommandResult deleteSession(const QString& sessionId);
    Q_INVOKABLE SessionCommandResult renameSession(const QString& sessionId, const QString& name);
    Q_INVOKABLE SessionCommandResult addDevice(
        const QString& sessionId,
        const QString& deviceId,
        const QString& role = QString());
    Q_INVOKABLE SessionCommandResult removeDevice(const QString& sessionId, const QString& deviceId);
    Q_INVOKABLE SessionCommandResult setDeviceEnabled(const QString& sessionId, const QString& deviceId, bool enabled);
    Q_INVOKABLE SessionCommandResult setDeviceRole(const QString& sessionId, const QString& deviceId, const QString& role);
    Q_INVOKABLE SessionCommandResult setSource(const QString& sessionId, const QString& sourceId);
    Q_INVOKABLE SessionCommandResult activateSession(const QString& sessionId);
    Q_INVOKABLE SessionCommandResult deactivateSession(const QString& sessionId);
    Q_INVOKABLE SessionCommandResult retrySession(const QString& sessionId);
    Q_INVOKABLE SessionCommandResult setGroupVolume(const QString& sessionId, double value);
    Q_INVOKABLE SessionCommandResult setSessionMuted(const QString& sessionId, bool muted);
    Q_INVOKABLE SessionCommandResult setDeviceVolume(const QString& sessionId, const QString& deviceId, double value);
    Q_INVOKABLE SessionCommandResult setDeviceMuted(const QString& sessionId, const QString& deviceId, bool muted);
    Q_INVOKABLE SessionCommandResult setAutoReconnect(const QString& sessionId, bool enabled);
    Q_INVOKABLE SessionCommandResult setRecoveryPolicy(const QString& sessionId, const QString& policy);
    Q_INVOKABLE SessionCommandResult restoreLastSession();
    Q_INVOKABLE QString duplicateSession(const QString& sessionId);
    Q_INVOKABLE QString commandResultText(int result) const;
    Q_INVOKABLE QString sourceDisplayName(const QString& sourceId) const;
    Q_INVOKABLE QString sessionStateLabel(const QString& sessionId) const;

    void refreshActiveSession();

    /// Test hook: when set, used instead of BluetoothManager::requestManagedReconnect.
    void setManagedReconnectHookForTest(std::function<void(const QString& devicePath)> hook);
    QVector<QString> takeManagedReconnectRequestsForTest();

signals:
    void sessionsChanged();
    void sessionAdded(const QString& sessionId);
    void sessionRemoved(const QString& sessionId);
    void sessionUpdated(const QString& sessionId);
    void sessionStateChanged(const QString& sessionId, auralis::session::SessionState oldState, auralis::session::SessionState newState);
    void sessionError(const QString& sessionId, auralis::session::SessionError error, const QString& detail);
    void currentSessionIdChanged();
    void sessionStateTextChanged();
    void groupVolumeChanged();
    void currentSourceIdChanged();
    void currentMutedChanged();

private slots:
    void handleExternalGraphChanged();
    void handleRouteStateChanged(const QString& routeId, auralis::audio::RouteState state);
    void handleRouteRemoved(const QString& routeId);
    void handleDeviceRegistryChanged();
    void handleRecoveryTick();
    void handleManagedReconnectExhausted(const QString& devicePath, int attempts, const QString& reason);
    void handleManagedReconnectTerminalFailure(const QString& devicePath, int attempts, const QString& reason);

private:
    AuralisSession* mutableSession(const QString& id);
    void emitSessionUiSignals();
    void touchUpdated(AuralisSession& session);
    SessionCommandResult persistAll(const QString& sessionIdForError = QString());
    void loadSessions();
    SessionIntent currentIntent(const AuralisSession& session) const;
    void recomputeSession(AuralisSession& session);
    void reconcileActiveSession(AuralisSession& session);
    void stopSession(AuralisSession& session, bool persist);
    void bumpGeneration(AuralisSession& session);
    QString findActiveSessionId() const;
    bool isActiveLifecycleState(SessionState state) const;
    bool allowsRouteCreation(SessionState state) const;
    bool policyAllowsAutoRouteRestore(const AuralisSession& session) const;
    bool policyAllowsBluetoothReconnect(const AuralisSession& session) const;
    void applyAutoRestoreFlags(AuralisSession& session);
    void requestManagedReconnect(AuralisSession& session, SessionDevice& device);
    void cancelRecovery(const QString& sessionId, const QString& deviceId);
    void cancelAllRecovery(AuralisSession& session);
    void installReconnectSuppressions(const AuralisSession& session);
    void removeReconnectSuppressions(const AuralisSession& session);
    void clearStaleMemberErrors(AuralisSession& session);
    QString canonicalMemberDeviceId(const QString& deviceId) const;
    QString devicePathForAddress(const QString& address) const;
    SessionCommandResult validateVolume(double value) const;
    void setupUiModels();

    auralis::bluetooth::BluetoothManager* bluetooth_ = nullptr;
    auralis::audio::PipeWireManager* pipeWire_ = nullptr;
    auralis::audio::AudioRouter* router_ = nullptr;
    auralis::audio::AudioEndpointRegistry* endpoints_ = nullptr;
    auralis::bluetooth::DeviceRegistry* deviceRegistry_ = nullptr;
    std::unique_ptr<SessionPersistence> persistence_;
    std::unique_ptr<RoutingCoordinator> routing_;
    std::unique_ptr<VolumeCoordinator> volume_;
    QVector<AuralisSession> sessions_;
    QString activeSessionId_;
    QHash<QString, quint64> generations_;
    QTimer recoverySweep_;
    quint64 nextGeneration_ = 1;
    auralis::core::ServiceStatus status_ = auralis::core::ServiceStatus::Uninitialized;
    bool reconciling_ = false;
    bool reconcilePending_ = false;
    int routeTeardownDepth_ = 0;
    bool persistenceDirty_ = false;
    std::function<void(const QString&)> managedReconnectHook_;
    QVector<QString> managedReconnectRequestsForTest_;
    SessionListModel* sessionList_ = nullptr;
    SessionMemberListModel* sessionMembers_ = nullptr;
    SessionMemberListModel* currentMembers_ = nullptr;
    SelectedSessionViewModel* selectedSession_ = nullptr;
    bool uiSignalsPrimed_ = false;
    QString lastEmittedSessionId_;
    QString lastEmittedStateText_;
    double lastEmittedGroupVolume_ = -1.0;
    QString lastEmittedSourceId_;
    bool lastEmittedMuted_ = false;
};

} // namespace auralis::session

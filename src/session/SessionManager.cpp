#include <auralis/session/SessionListModel.h>
#include <auralis/session/SelectedSessionViewModel.h>
#include <auralis/session/SessionManager.h>

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/IAudioManager.h>
#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/bluetooth/BlueZTypes.h>
#include <auralis/bluetooth/IBluetoothManager.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/LoggingCategories.h>
#include <auralis/session/SessionStateMachine.h>

#include <QStandardPaths>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace auralis::session {
namespace {

double clampFiniteVolume(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

QString defaultPersistencePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/sessions.json");
}

} // namespace

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
    , persistence_(std::make_unique<SessionPersistence>(defaultPersistencePath()))
{
    setupUiModels();
}

SessionManager::SessionManager(
    auralis::bluetooth::IBluetoothManager* bluetooth,
    auralis::audio::IAudioManager* audio,
    const QString& persistencePath,
    QObject* parent)
    : QObject(parent)
    , bluetooth_(bluetooth)
    , audio_(audio)
    , persistence_(std::make_unique<SessionPersistence>(
          persistencePath.isEmpty() ? defaultPersistencePath() : persistencePath))
{
    if (audio_ != nullptr) {
        router_ = audio_->audioRouter();
        endpoints_ = audio_->endpointRegistry();
    }
    if (bluetooth_ != nullptr) {
        deviceRegistry_ = bluetooth_->deviceRegistry();
    }
    setupUiModels();
}

SessionManager::SessionManager(
    auralis::audio::AudioRouter* router,
    auralis::audio::AudioEndpointRegistry* endpoints,
    auralis::bluetooth::DeviceRegistry* devices,
    auralis::bluetooth::IBluetoothManager* bluetooth,
    const QString& persistencePath,
    QObject* parent)
    : QObject(parent)
    , bluetooth_(bluetooth)
    , router_(router)
    , endpoints_(endpoints)
    , deviceRegistry_(devices)
    , persistence_(std::make_unique<SessionPersistence>(
          persistencePath.isEmpty() ? defaultPersistencePath() : persistencePath))
{
    setupUiModels();
}

SessionManager::~SessionManager()
{
    shutdown();
}

bool SessionManager::initialize()
{
    if (status_ == auralis::core::ServiceStatus::Ready) {
        return true;
    }
    status_ = auralis::core::ServiceStatus::Initializing;

    if (audio_ != nullptr) {
        router_ = audio_->audioRouter();
        endpoints_ = audio_->endpointRegistry();
    }
    if (bluetooth_ != nullptr) {
        deviceRegistry_ = bluetooth_->deviceRegistry();
    }

    if (router_ != nullptr) {
        routing_ = std::make_unique<RoutingCoordinator>(router_, endpoints_, deviceRegistry_);
        volume_ = std::make_unique<VolumeCoordinator>(router_);
        connect(router_, &auralis::audio::AudioRouter::routeStateChanged, this, &SessionManager::handleRouteStateChanged);
        connect(router_, &auralis::audio::AudioRouter::routeRemoved, this, &SessionManager::handleRouteRemoved);
        connect(router_, &auralis::audio::AudioRouter::routeError, this, [this](const QString& routeId, auralis::audio::RouteError, const QString& detail) {
            if (activeSessionId_.isEmpty()) {
                return;
            }
            if (AuralisSession* session = mutableSession(activeSessionId_)) {
                for (SessionDevice& device : session->devices) {
                    if (device.runtime.routeId == routeId) {
                        device.runtime.lastError = {SessionError::RouteActivationFailed, detail};
                        emit sessionError(session->id, SessionError::RouteActivationFailed, detail);
                        break;
                    }
                }
            }
        });
    }
    if (audio_ != nullptr && audio_->uiObject() != nullptr) {
        QObject::connect(
            audio_->uiObject(),
            SIGNAL(graphRevisionChanged()),
            this,
            SLOT(handleExternalGraphChanged()));
    }
    if (deviceRegistry_ != nullptr) {
        connect(deviceRegistry_, &auralis::bluetooth::DeviceRegistry::deviceUpdated, this, &SessionManager::handleDeviceRegistryChanged);
        connect(deviceRegistry_, &auralis::bluetooth::DeviceRegistry::deviceRemoved, this, &SessionManager::handleDeviceRegistryChanged);
        connect(deviceRegistry_, &auralis::bluetooth::DeviceRegistry::deviceAdded, this, &SessionManager::handleDeviceRegistryChanged);
    }
    if (bluetooth_ != nullptr && bluetooth_->uiObject() != nullptr) {
        QObject::connect(
            bluetooth_->uiObject(),
            SIGNAL(managedReconnectExhausted(QString,int,QString)),
            this,
            SLOT(handleManagedReconnectExhausted(QString,int,QString)));
        QObject::connect(
            bluetooth_->uiObject(),
            SIGNAL(managedReconnectTerminalFailure(QString,int,QString)),
            this,
            SLOT(handleManagedReconnectTerminalFailure(QString,int,QString)));
    }

    recoverySweep_.setInterval(1000);
    connect(&recoverySweep_, &QTimer::timeout, this, &SessionManager::handleRecoveryTick);

    loadSessions();
    if (sessionList_ != nullptr) {
        sessionList_->reload();
    }
    status_ = auralis::core::ServiceStatus::Ready;
    qCInfo(auralisSession) << "Session manager initialized sessions=" << sessions_.size();
    emitSessionUiSignals();
    return true;
}

void SessionManager::shutdown()
{
    if (status_ == auralis::core::ServiceStatus::Uninitialized) {
        return;
    }

    recoverySweep_.stop();
    for (AuralisSession& session : sessions_) {
        if (isActiveLifecycleState(session.state) || session.state == SessionState::Failed) {
            bumpGeneration(session);
            cancelAllRecovery(session);
            stopSession(session, false);
        }
    }
    persistAll();
    activeSessionId_.clear();
    status_ = auralis::core::ServiceStatus::Uninitialized;
    qCInfo(auralisSession) << "Session manager shut down";
    emitSessionUiSignals();
}

auralis::core::ServiceStatus SessionManager::status() const noexcept
{
    return status_;
}

QObject* SessionManager::uiObject()
{
    return this;
}

int SessionManager::sessionCount() const
{
    return static_cast<int>(sessions_.size());
}

QString SessionManager::currentSessionId() const
{
    return activeSessionId_;
}

QString SessionManager::sessionStateText() const
{
    if (activeSessionId_.isEmpty()) {
        return userFacingSessionState(SessionState::Idle);
    }
    for (const AuralisSession& session : sessions_) {
        if (session.id == activeSessionId_) {
            return userFacingSessionState(session.state);
        }
    }
    return userFacingSessionState(SessionState::Idle);
}

double SessionManager::groupVolume() const
{
    for (const AuralisSession& session : sessions_) {
        if (session.id == activeSessionId_) {
            return session.groupVolume;
        }
    }
    return 1.0;
}

QString SessionManager::currentSourceId() const
{
    for (const AuralisSession& session : sessions_) {
        if (session.id == activeSessionId_) {
            return session.sourceId;
        }
    }
    return {};
}

bool SessionManager::currentMuted() const
{
    for (const AuralisSession& session : sessions_) {
        if (session.id == activeSessionId_) {
            return session.muted;
        }
    }
    return false;
}

QAbstractItemModel* SessionManager::sessionList() const
{
    return sessionList_;
}

QAbstractItemModel* SessionManager::sessionMembers() const
{
    return sessionMembers_;
}

QAbstractItemModel* SessionManager::currentMembers() const
{
    return currentMembers_;
}

QObject* SessionManager::selectedSession() const
{
    return selectedSession_;
}

auralis::bluetooth::DeviceRegistry* SessionManager::deviceRegistry() const noexcept
{
    return deviceRegistry_;
}

void SessionManager::setupUiModels()
{
    if (sessionList_ == nullptr) {
        sessionList_ = new SessionListModel(this, this);
    }
    if (sessionMembers_ == nullptr) {
        sessionMembers_ = new SessionMemberListModel(this, this);
    }
    if (currentMembers_ == nullptr) {
        currentMembers_ = new SessionMemberListModel(this, this);
        connect(this, &SessionManager::currentSessionIdChanged, this, [this]() {
            currentMembers_->setSessionId(activeSessionId_);
        });
    }
    if (selectedSession_ == nullptr) {
        selectedSession_ = new SelectedSessionViewModel(this, this);
    }
}

QString SessionManager::commandResultText(int result) const
{
    return toString(static_cast<SessionCommandResult>(result));
}

QString SessionManager::sourceDisplayName(const QString& sourceId) const
{
    if (router_ == nullptr) {
        return sourceId;
    }
    return router_->sourceDisplayName(sourceId);
}

QString SessionManager::sessionStateLabel(const QString& sessionId) const
{
    const std::optional<AuralisSession> session = sessionById(sessionId);
    if (!session.has_value()) {
        return userFacingSessionState(SessionState::Idle);
    }
    return userFacingSessionState(session->state);
}

QVector<AuralisSession> SessionManager::sessions() const
{
    return sessions_;
}

std::optional<AuralisSession> SessionManager::sessionById(const QString& id) const
{
    for (const AuralisSession& session : sessions_) {
        if (session.id == id) {
            return session;
        }
    }
    return std::nullopt;
}

void SessionManager::setManagedReconnectHookForTest(std::function<void(const QString&)> hook)
{
    managedReconnectHook_ = std::move(hook);
}

QVector<QString> SessionManager::takeManagedReconnectRequestsForTest()
{
    QVector<QString> out = managedReconnectRequestsForTest_;
    managedReconnectRequestsForTest_.clear();
    return out;
}

QString SessionManager::createSession(const QString& name)
{
    qCInfo(auralisSession) << "SessionCreateRequested name=" << name.trimmed();
    AuralisSession session;
    session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.name = name.trimmed();
    if (session.name.isEmpty()) {
        session.name = QStringLiteral("Session");
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    session.createdAt = now;
    session.updatedAt = now;
    session.state = SessionState::Idle;
    sessions_.push_back(session);
    const SessionCommandResult persistResult = persistAll(session.id);
    if (persistResult != SessionCommandResult::Accepted) {
        qCWarning(auralisSession) << "SessionPersistenceCreateFailed id=" << session.id << "name=" << session.name;
        sessions_.pop_back();
        return {};
    }
    qCInfo(auralisSession) << "SessionCreated id=" << session.id << "name=" << session.name;
    emit sessionAdded(session.id);
    emit sessionsChanged();
    emitSessionUiSignals();
    return session.id;
}

SessionCommandResult SessionManager::deleteSession(const QString& sessionId)
{
    qCInfo(auralisSession) << "SessionDeleteRequested id=" << sessionId;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    bumpGeneration(*session);
    cancelAllRecovery(*session);
    if (isActiveLifecycleState(session->state) || session->state == SessionState::Failed) {
        stopSession(*session, false);
    }
    if (activeSessionId_ == sessionId) {
        activeSessionId_.clear();
    }
    generations_.remove(sessionId);
    for (int i = 0; i < sessions_.size(); ++i) {
        if (sessions_.at(i).id == sessionId) {
            sessions_.removeAt(i);
            break;
        }
    }
    const SessionCommandResult persistResult = persistAll(sessionId);
    emit sessionRemoved(sessionId);
    emit sessionsChanged();
    emitSessionUiSignals();
    return persistResult;
}

SessionCommandResult SessionManager::renameSession(const QString& sessionId, const QString& name)
{
    qCInfo(auralisSession) << "SessionRenameRequested id=" << sessionId << "name=" << name.trimmed();
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return SessionCommandResult::InvalidSessionName;
    }
    session->name = trimmed;
    touchUpdated(*session);
    const SessionCommandResult persistResult = persistAll(sessionId);
    emit sessionUpdated(sessionId);
    return persistResult;
}

SessionCommandResult SessionManager::addDevice(const QString& sessionId, const QString& deviceId, const QString& role)
{
    qCInfo(auralisSession) << "SessionAddDeviceRequested session=" << sessionId
                           << "device=" << deviceId << "role=" << role;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    const QString normalized = canonicalMemberDeviceId(deviceId);
    if (normalized.isEmpty()) {
        return SessionCommandResult::MemberNotFound;
    }
    for (const SessionDevice& device : session->devices) {
        if (device.deviceId == normalized) {
            return SessionCommandResult::DuplicateMember;
        }
    }
    SessionDevice device;
    device.deviceId = normalized;
    bool ok = false;
    device.role = sessionDeviceRoleFromString(role, &ok);
    device.runtime.autoRestoreAllowed = session->state == SessionState::Starting
        || policyAllowsAutoRouteRestore(*session);
    session->devices.push_back(device);
    if (sessionId == activeSessionId_ && bluetooth_ != nullptr) {
        const QString path = devicePathForAddress(normalized);
        if (!path.isEmpty()) {
            bluetooth_->suppressAutoReconnect(path);
        }
    }
    touchUpdated(*session);
    const SessionCommandResult persistResult = persistAll(sessionId);
    emit sessionUpdated(sessionId);
    if (sessionId == activeSessionId_ && allowsRouteCreation(session->state)) {
        reconcileActiveSession(*session);
    }
    return persistResult;
}

SessionCommandResult SessionManager::removeDevice(const QString& sessionId, const QString& deviceId)
{
    qCInfo(auralisSession) << "SessionRemoveDeviceRequested session=" << sessionId
                           << "device=" << deviceId;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    const QString normalized = deviceId.trimmed().toUpper();
    for (int i = 0; i < session->devices.size(); ++i) {
        SessionDevice& device = session->devices[i];
        if (device.deviceId != normalized) {
            continue;
        }
        cancelRecovery(sessionId, normalized);
        if (sessionId == activeSessionId_ && bluetooth_ != nullptr) {
            const QString path = devicePathForAddress(normalized);
            if (!path.isEmpty()) {
                bluetooth_->unsuppressAutoReconnect(path);
            }
        }
        if (!device.runtime.routeId.isEmpty() && router_ != nullptr) {
            router_->deactivateRoute(device.runtime.routeId);
            router_->removeRoute(device.runtime.routeId);
            device.runtime.routeId.clear();
            device.runtime.routeActive = false;
        }
        session->devices.removeAt(i);
        touchUpdated(*session);
        const SessionCommandResult persistResult = persistAll(sessionId);
        emit sessionUpdated(sessionId);
        if (sessionId == activeSessionId_ && allowsRouteCreation(session->state)) {
            reconcileActiveSession(*session);
        }
        return persistResult;
    }
    return SessionCommandResult::MemberNotFound;
}

SessionCommandResult SessionManager::setDeviceEnabled(const QString& sessionId, const QString& deviceId, bool enabled)
{
    qCInfo(auralisSession) << "SessionDeviceEnabledRequested session=" << sessionId
                           << "device=" << deviceId << "enabled=" << enabled;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    const QString normalizedId = deviceId.trimmed().toUpper();
    int index = -1;
    for (int i = 0; i < session->devices.size(); ++i) {
        if (session->devices.at(i).deviceId == normalizedId) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        return SessionCommandResult::MemberNotFound;
    }

    session->devices[index].enabled = enabled;
    if (!enabled) {
        cancelRecovery(sessionId, normalizedId);
        // Copy before route teardown: deactivate/remove may re-enter and detach devices.
        const QString routeId = session->devices.at(index).runtime.routeId;
        const QString address = session->devices.at(index).deviceId;
        session->devices[index].runtime.autoRestoreAllowed = false;
        session->devices[index].runtime.recovering = false;
        if (!routeId.isEmpty() && router_ != nullptr) {
            router_->deactivateRoute(routeId);
            router_->removeRoute(routeId);
            session = mutableSession(sessionId);
            if (session == nullptr) {
                return SessionCommandResult::SessionNotFound;
            }
            index = -1;
            for (int i = 0; i < session->devices.size(); ++i) {
                if (session->devices.at(i).deviceId == normalizedId) {
                    index = i;
                    break;
                }
            }
            if (index < 0) {
                return SessionCommandResult::MemberNotFound;
            }
            session->devices[index].runtime.routeId.clear();
            session->devices[index].runtime.routeActive = false;
            session->devices[index].runtime.routeRequested = false;
        }
        if (sessionId == activeSessionId_ && bluetooth_ != nullptr) {
            const QString path = devicePathForAddress(address);
            if (!path.isEmpty()) {
                bluetooth_->unsuppressAutoReconnect(path);
            }
        }
    } else {
        session->devices[index].runtime.autoRestoreAllowed = policyAllowsAutoRouteRestore(*session)
            || session->state == SessionState::Starting;
        if (sessionId == activeSessionId_ && bluetooth_ != nullptr) {
            const QString path = devicePathForAddress(session->devices.at(index).deviceId);
            if (!path.isEmpty()) {
                bluetooth_->suppressAutoReconnect(path);
            }
        }
    }
    touchUpdated(*session);
    const SessionCommandResult persistResult = persistAll(sessionId);
    emit sessionUpdated(sessionId);
    session = mutableSession(sessionId);
    if (session != nullptr && sessionId == activeSessionId_ && allowsRouteCreation(session->state)) {
        reconcileActiveSession(*session);
    }
    return persistResult;
}

SessionCommandResult SessionManager::setDeviceRole(const QString& sessionId, const QString& deviceId, const QString& role)
{
    qCInfo(auralisSession) << "SessionDeviceRoleRequested session=" << sessionId
                           << "device=" << deviceId << "role=" << role;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    for (SessionDevice& device : session->devices) {
        if (device.deviceId != deviceId.trimmed().toUpper()) {
            continue;
        }
        bool ok = false;
        device.role = sessionDeviceRoleFromString(role, &ok);
        if (!ok && !role.trimmed().isEmpty()) {
            return SessionCommandResult::InternalError;
        }
        touchUpdated(*session);
        const SessionCommandResult persistResult = persistAll(sessionId);
        emit sessionUpdated(sessionId);
        return persistResult;
    }
    return SessionCommandResult::MemberNotFound;
}

SessionCommandResult SessionManager::setSource(const QString& sessionId, const QString& sourceId)
{
    qCInfo(auralisSession) << "SessionSourceRequested session=" << sessionId << "source=" << sourceId;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    session->sourceId = sourceId.trimmed();
    touchUpdated(*session);
    const SessionCommandResult persistResult = persistAll(sessionId);
    emit sessionUpdated(sessionId);
    emitSessionUiSignals();
    if (sessionId == activeSessionId_ && allowsRouteCreation(session->state)) {
        bumpGeneration(*session);
        ++routeTeardownDepth_;
        if (routing_ != nullptr) {
            routing_->stopSessionRoutes(*session);
        }
        --routeTeardownDepth_;
        for (SessionDevice& device : session->devices) {
            device.runtime.autoRestoreAllowed = true;
            device.runtime.routeRequested = false;
        }
        reconcileActiveSession(*session);
    }
    return persistResult;
}

SessionCommandResult SessionManager::activateSession(const QString& sessionId)
{
    qCInfo(auralisSession) << "SessionActivationRequested id=" << sessionId;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    if (session->sourceId.isEmpty()) {
        return SessionCommandResult::NoSourceConfigured;
    }
    if (session->devices.isEmpty()
        || !std::any_of(session->devices.cbegin(), session->devices.cend(), [](const SessionDevice& d) { return d.enabled; })) {
        return SessionCommandResult::NoMembersConfigured;
    }
    const QString otherActive = findActiveSessionId();
    if (!otherActive.isEmpty() && otherActive != sessionId) {
        return SessionCommandResult::AlreadyActive;
    }
    if (allowsRouteCreation(session->state) && sessionId == activeSessionId_) {
        return SessionCommandResult::Accepted;
    }

    // Switching away from Failed: bump and clear.
    if (session->state == SessionState::Failed) {
        cancelAllRecovery(*session);
        if (router_ != nullptr) {
            for (SessionDevice& device : session->devices) {
                if (device.runtime.routeId.isEmpty()) {
                    continue;
                }
                if (const std::optional<auralis::audio::AudioRoute> route = router_->routeById(device.runtime.routeId)) {
                    if (route->state == auralis::audio::RouteState::Failed) {
                        router_->removeRoute(device.runtime.routeId);
                        device.runtime.routeId.clear();
                        device.runtime.routeRequested = false;
                        device.runtime.routeActive = false;
                    }
                }
            }
        }
    }

    bumpGeneration(*session);
    activeSessionId_ = sessionId;
    const SessionState oldState = session->state;
    session->state = SessionState::Starting;
    session->restoreIntent = false;
    session->error = {};
    for (SessionDevice& device : session->devices) {
        device.runtime.autoRestoreAllowed = true;
        device.runtime.managedReconnectRequested = false;
        device.runtime.recovering = false;
        device.runtime.lastError = {};
    }
    touchUpdated(*session);
    persistAll(sessionId);
    emit sessionStateChanged(sessionId, oldState, SessionState::Starting);
    emit sessionUpdated(sessionId);
    emitSessionUiSignals();

    installReconnectSuppressions(*session);

    if (!recoverySweep_.isActive()) {
        recoverySweep_.start();
    }
    reconcileActiveSession(*session);
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::deactivateSession(const QString& sessionId)
{
    qCInfo(auralisSession) << "SessionDeactivationRequested id=" << sessionId;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    if (!isActiveLifecycleState(session->state) && session->state != SessionState::Failed) {
        return SessionCommandResult::Accepted;
    }
    bumpGeneration(*session);
    cancelAllRecovery(*session);
    stopSession(*session, true);
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::retrySession(const QString& sessionId)
{
    qCInfo(auralisSession) << "SessionRetryRequested id=" << sessionId;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    if (session->state != SessionState::Failed && session->state != SessionState::Degraded) {
        return SessionCommandResult::InvalidState;
    }
    for (SessionDevice& device : session->devices) {
        device.runtime.autoRestoreAllowed = true;
        device.runtime.managedReconnectRequested = false;
        device.runtime.lastError = {};
        device.runtime.recovering = false;
    }
    session->error = {};
    if (session->state == SessionState::Degraded) {
        session->state = SessionState::Failed;
    }
    return activateSession(sessionId);
}

SessionCommandResult SessionManager::setGroupVolume(const QString& sessionId, double value)
{
    qCInfo(auralisSession) << "SessionGroupVolumeRequested id=" << sessionId << "volume=" << value;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    const SessionCommandResult volumeResult = validateVolume(value);
    if (volumeResult != SessionCommandResult::Accepted) {
        return volumeResult;
    }
    session->groupVolume = clampFiniteVolume(value);
    touchUpdated(*session);
    const SessionCommandResult persistResult = persistAll(sessionId);
    if (volume_ != nullptr) {
        volume_->applySessionVolumes(*session);
    }
    emit sessionUpdated(sessionId);
    emitSessionUiSignals();
    return persistResult;
}

SessionCommandResult SessionManager::setSessionMuted(const QString& sessionId, bool muted)
{
    qCInfo(auralisSession) << "SessionMuteRequested id=" << sessionId << "muted=" << muted;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    session->muted = muted;
    touchUpdated(*session);
    const SessionCommandResult persistResult = persistAll(sessionId);
    if (volume_ != nullptr) {
        volume_->applySessionVolumes(*session);
    }
    emit sessionUpdated(sessionId);
    emitSessionUiSignals();
    return persistResult;
}

SessionCommandResult SessionManager::setDeviceVolume(const QString& sessionId, const QString& deviceId, double value)
{
    qCInfo(auralisSession) << "SessionDeviceVolumeRequested session=" << sessionId
                           << "device=" << deviceId << "volume=" << value;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    const SessionCommandResult volumeResult = validateVolume(value);
    if (volumeResult != SessionCommandResult::Accepted) {
        return volumeResult;
    }
    for (SessionDevice& device : session->devices) {
        if (device.deviceId != deviceId.trimmed().toUpper()) {
            continue;
        }
        device.volumeTrim = clampFiniteVolume(value);
        touchUpdated(*session);
        const SessionCommandResult persistResult = persistAll(sessionId);
        if (volume_ != nullptr) {
            volume_->applyMemberVolume(*session, device);
        }
        emit sessionUpdated(sessionId);
        return persistResult;
    }
    return SessionCommandResult::MemberNotFound;
}

SessionCommandResult SessionManager::setDeviceMuted(const QString& sessionId, const QString& deviceId, bool muted)
{
    qCInfo(auralisSession) << "SessionDeviceMuteRequested session=" << sessionId
                           << "device=" << deviceId << "muted=" << muted;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    for (SessionDevice& device : session->devices) {
        if (device.deviceId != deviceId.trimmed().toUpper()) {
            continue;
        }
        device.muted = muted;
        touchUpdated(*session);
        const SessionCommandResult persistResult = persistAll(sessionId);
        if (volume_ != nullptr) {
            volume_->applyMemberVolume(*session, device);
        }
        emit sessionUpdated(sessionId);
        return persistResult;
    }
    return SessionCommandResult::MemberNotFound;
}

SessionCommandResult SessionManager::setAutoReconnect(const QString& sessionId, bool enabled)
{
    qCInfo(auralisSession) << "SessionAutoReconnectRequested id=" << sessionId << "enabled=" << enabled;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    session->autoReconnect = enabled;
    if (!enabled) {
        bumpGeneration(*session);
        for (SessionDevice& device : session->devices) {
            if (device.runtime.managedReconnectRequested) {
                cancelRecovery(sessionId, device.deviceId);
            }
        }
    }
    touchUpdated(*session);
    const SessionCommandResult persistResult = persistAll(sessionId);
    emit sessionUpdated(sessionId);
    if (sessionId == activeSessionId_ && allowsRouteCreation(session->state)) {
        reconcileActiveSession(*session);
    }
    return persistResult;
}

SessionCommandResult SessionManager::setRecoveryPolicy(const QString& sessionId, const QString& policy)
{
    qCInfo(auralisSession) << "SessionRecoveryPolicyRequested id=" << sessionId << "policy=" << policy;
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    bool ok = false;
    const RecoveryPolicy next = recoveryPolicyFromString(policy, &ok);
    if (!ok) {
        return SessionCommandResult::InternalError;
    }
    session->recoveryPolicy = next;
    bumpGeneration(*session);
    if (next == RecoveryPolicy::None) {
        cancelAllRecovery(*session);
        for (SessionDevice& device : session->devices) {
            if (!device.runtime.routeActive) {
                device.runtime.autoRestoreAllowed = false;
                device.runtime.recovering = false;
            }
        }
    } else if (next == RecoveryPolicy::RestoreRoutesOnly) {
        for (SessionDevice& device : session->devices) {
            if (device.runtime.managedReconnectRequested) {
                const QString path = devicePathForAddress(device.deviceId);
                if (!path.isEmpty() && bluetooth_ != nullptr) {
                    bluetooth_->cancelManagedReconnect(path);
                }
                device.runtime.managedReconnectRequested = false;
            }
            if (device.enabled) {
                device.runtime.autoRestoreAllowed = true;
            }
        }
    } else {
        for (SessionDevice& device : session->devices) {
            if (device.enabled) {
                device.runtime.autoRestoreAllowed = true;
            }
        }
    }
    touchUpdated(*session);
    const SessionCommandResult persistResult = persistAll(sessionId);
    emit sessionUpdated(sessionId);
    if (sessionId == activeSessionId_ && allowsRouteCreation(session->state)) {
        reconcileActiveSession(*session);
    }
    return persistResult;
}

SessionCommandResult SessionManager::restoreLastSession()
{
    qCInfo(auralisSession) << "SessionRestoreLastRequested";
    QString candidateId;
    QDateTime latest;
    for (const AuralisSession& session : sessions_) {
        if (!session.restoreIntent || !session.lastUsedAt.isValid()) {
            continue;
        }
        if (!candidateId.isEmpty() && session.lastUsedAt < latest) {
            continue;
        }
        candidateId = session.id;
        latest = session.lastUsedAt;
    }
    if (candidateId.isEmpty()) {
        for (const AuralisSession& session : sessions_) {
            if (!session.lastUsedAt.isValid()) {
                continue;
            }
            if (!candidateId.isEmpty() && session.lastUsedAt < latest) {
                continue;
            }
            candidateId = session.id;
            latest = session.lastUsedAt;
        }
    }
    if (candidateId.isEmpty()) {
        return SessionCommandResult::SessionNotFound;
    }
    return activateSession(candidateId);
}

QString SessionManager::duplicateSession(const QString& sessionId)
{
    const AuralisSession* original = nullptr;
    for (const AuralisSession& session : sessions_) {
        if (session.id == sessionId) {
            original = &session;
            break;
        }
    }
    if (original == nullptr) {
        return {};
    }

    auto nameTaken = [this](const QString& name) {
        for (const AuralisSession& session : sessions_) {
            if (session.name == name) {
                return true;
            }
        }
        return false;
    };

    QString copyName = original->name + QStringLiteral(" Copy");
    if (nameTaken(copyName)) {
        int suffix = 2;
        while (nameTaken(original->name + QStringLiteral(" Copy %1").arg(suffix))) {
            ++suffix;
        }
        copyName = original->name + QStringLiteral(" Copy %1").arg(suffix);
    }

    AuralisSession session;
    session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.name = copyName;
    session.sourceId = original->sourceId;
    session.devices = original->devices;
    for (SessionDevice& device : session.devices) {
        device.runtime = {};
    }
    session.state = SessionState::Idle;
    session.error = {};
    session.groupVolume = original->groupVolume;
    session.muted = original->muted;
    session.autoReconnect = original->autoReconnect;
    session.recoveryPolicy = original->recoveryPolicy;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    session.createdAt = now;
    session.updatedAt = now;
    session.lastUsedAt = {};
    session.restoreIntent = false;
    session.operationGeneration = 0;
    sessions_.push_back(session);
    const SessionCommandResult persistResult = persistAll(session.id);
    if (persistResult != SessionCommandResult::Accepted) {
        sessions_.pop_back();
        return {};
    }
    qCInfo(auralisSession) << "SessionDuplicated id=" << session.id << "from=" << sessionId;
    emit sessionAdded(session.id);
    emit sessionsChanged();
    emitSessionUiSignals();
    return session.id;
}

int SessionManager::auralisRouteCountForDevice(const QString& deviceId) const
{
    const QString canonical = canonicalMemberDeviceId(deviceId);
    if (canonical.isEmpty()) {
        return 0;
    }
    int count = 0;
    for (const AuralisSession& session : sessions_) {
        for (const SessionDevice& device : session.devices) {
            if (device.deviceId == canonical && !device.runtime.routeId.isEmpty()) {
                ++count;
            }
        }
    }
    return count;
}

SessionCommandResult SessionManager::releaseDeviceFromAuralis(const QString& deviceId)
{
    const QString canonical = canonicalMemberDeviceId(deviceId);
    if (canonical.isEmpty()) {
        qCWarning(auralisSession) << "AppDeviceReleaseRejected device=" << deviceId << "reason=unknown-device";
        return SessionCommandResult::MemberNotFound;
    }

    QStringList sessionsToDisable;
    for (const AuralisSession& session : sessions_) {
        for (const SessionDevice& device : session.devices) {
            if (device.deviceId == canonical && device.enabled) {
                sessionsToDisable.push_back(session.id);
                break;
            }
        }
    }
    if (sessionsToDisable.isEmpty()) {
        qCInfo(auralisSession) << "AppDeviceReleaseNoOp device=" << canonical;
        return SessionCommandResult::Accepted;
    }

    qCInfo(auralisSession) << "AppDeviceReleaseRequested device=" << canonical
                           << "sessions=" << sessionsToDisable.size();
    SessionCommandResult result = SessionCommandResult::Accepted;
    for (const QString& sessionId : sessionsToDisable) {
        const SessionCommandResult itemResult = setDeviceEnabled(sessionId, canonical, false);
        if (itemResult != SessionCommandResult::Accepted) {
            result = itemResult;
        }
    }
    qCInfo(auralisSession) << "AppDeviceReleaseCompleted device=" << canonical
                           << "result=" << toString(result);
    return result;
}

void SessionManager::refreshActiveSession()
{
    handleExternalGraphChanged();
}

void SessionManager::handleExternalGraphChanged()
{
    if (activeSessionId_.isEmpty()) {
        return;
    }
    if (AuralisSession* session = mutableSession(activeSessionId_)) {
        if (session->state == SessionState::Failed || session->state == SessionState::Idle) {
            return;
        }
        reconcileActiveSession(*session);
    }
}

void SessionManager::handleRouteStateChanged(const QString& routeId, auralis::audio::RouteState state)
{
    Q_UNUSED(routeId);
    if (activeSessionId_.isEmpty() || routeTeardownDepth_ > 0) {
        return;
    }
    switch (state) {
    case auralis::audio::RouteState::Planning:
    case auralis::audio::RouteState::Ready:
    case auralis::audio::RouteState::Activating:
    case auralis::audio::RouteState::Deactivating:
    case auralis::audio::RouteState::Failed:
        if (state == auralis::audio::RouteState::Failed) {
            if (AuralisSession* session = mutableSession(activeSessionId_)) {
                if (session->state == SessionState::Stopping || session->state == SessionState::Idle) {
                    return;
                }
                if (routing_ != nullptr) {
                    routing_->refreshRuntime(*session);
                }
                recomputeSession(*session);
            }
        }
        // Failed reconcile would call activateRoute synchronously and recurse. Graph updates retry later.
        return;
    default:
        break;
    }
    if (AuralisSession* session = mutableSession(activeSessionId_)) {
        if (session->state == SessionState::Stopping || session->state == SessionState::Idle
            || session->state == SessionState::Failed) {
            qCDebug(auralisSession) << "RouteCallbackIgnoredDuringStopping" << session->id << routeId
                                    << toString(session->state);
            return;
        }
        if (!allowsRouteCreation(session->state)) {
            return;
        }
        reconcileActiveSession(*session);
    }
}

void SessionManager::handleRouteRemoved(const QString& routeId)
{
    if (activeSessionId_.isEmpty()) {
        return;
    }
    if (AuralisSession* session = mutableSession(activeSessionId_)) {
        for (SessionDevice& device : session->devices) {
            if (device.runtime.routeId == routeId) {
                device.runtime.routeId.clear();
                device.runtime.routeActive = false;
            }
        }
        if (routeTeardownDepth_ > 0) {
            return;
        }
        if (session->state == SessionState::Stopping || session->state == SessionState::Idle
            || session->state == SessionState::Failed) {
            return;
        }
        reconcileActiveSession(*session);
    }
}

void SessionManager::handleDeviceRegistryChanged()
{
    if (activeSessionId_.isEmpty()) {
        return;
    }
    if (AuralisSession* session = mutableSession(activeSessionId_)) {
        if (session->state == SessionState::Failed || session->state == SessionState::Idle) {
            return;
        }
        reconcileActiveSession(*session);
    }
}

void SessionManager::handleRecoveryTick()
{
    if (activeSessionId_.isEmpty()) {
        recoverySweep_.stop();
        return;
    }
    if (AuralisSession* session = mutableSession(activeSessionId_)) {
        if (session->state == SessionState::Failed || session->state == SessionState::Idle
            || session->state == SessionState::Stopping) {
            return;
        }
        reconcileActiveSession(*session);
    }
}

void SessionManager::handleManagedReconnectExhausted(const QString& devicePath, int attempts, const QString& reason)
{
    const QString normalizedPath = devicePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return;
    }
    AuralisSession* session = activeSessionId_.isEmpty() ? nullptr : mutableSession(activeSessionId_);
    if (session == nullptr) {
        return;
    }
    if (session->state == SessionState::Stopping || session->state == SessionState::Idle
        || session->state == SessionState::Failed) {
        return;
    }
    bool matched = false;
    for (SessionDevice& device : session->devices) {
        if (devicePathForAddress(device.deviceId) != normalizedPath) {
            continue;
        }
        matched = true;
        device.runtime.recovering = false;
        device.runtime.managedReconnectRequested = false;
        device.runtime.lastError = {
            SessionError::RecoveryExhausted,
            QStringLiteral("Reconnect exhausted after %1 attempts: %2").arg(attempts).arg(reason)};
        qCInfo(auralisSession) << "SessionRecoveryExhausted session=" << session->id
                               << "device=" << device.deviceId << "attempts=" << attempts;
        emit sessionError(session->id, SessionError::RecoveryExhausted, device.runtime.lastError.detail);
        break;
    }
    if (!matched) {
        return;
    }
    session->error = {SessionError::RecoveryExhausted, reason};
    recomputeSession(*session);
    emit sessionUpdated(session->id);
}

void SessionManager::handleManagedReconnectTerminalFailure(const QString& devicePath, int attempts, const QString& reason)
{
    const QString normalizedPath = devicePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return;
    }
    AuralisSession* session = activeSessionId_.isEmpty() ? nullptr : mutableSession(activeSessionId_);
    if (session == nullptr) {
        return;
    }
    if (session->state == SessionState::Stopping || session->state == SessionState::Idle
        || session->state == SessionState::Failed) {
        return;
    }
    bool matched = false;
    for (SessionDevice& device : session->devices) {
        if (devicePathForAddress(device.deviceId) != normalizedPath) {
            continue;
        }
        matched = true;
        device.runtime.recovering = false;
        device.runtime.managedReconnectRequested = false;
        device.runtime.lastError = {
            SessionError::RecoveryExhausted,
            QStringLiteral("Reconnect terminal failure after %1 attempts: %2").arg(attempts).arg(reason)};
        qCInfo(auralisSession) << "SessionRecoveryTerminalFailure session=" << session->id
                               << "device=" << device.deviceId << "attempts=" << attempts;
        emit sessionError(session->id, SessionError::RecoveryExhausted, device.runtime.lastError.detail);
        break;
    }
    if (!matched) {
        return;
    }
    recomputeSession(*session);
    emit sessionUpdated(session->id);
}

AuralisSession* SessionManager::mutableSession(const QString& id)
{
    for (AuralisSession& session : sessions_) {
        if (session.id == id) {
            return &session;
        }
    }
    return nullptr;
}

void SessionManager::emitSessionUiSignals()
{
    const QString id = currentSessionId();
    const QString state = sessionStateText();
    const double volume = groupVolume();
    const QString source = currentSourceId();
    const bool muted = currentMuted();
    const bool primed = uiSignalsPrimed_;
    uiSignalsPrimed_ = true;

    if (!primed || lastEmittedSessionId_ != id) {
        lastEmittedSessionId_ = id;
        emit currentSessionIdChanged();
    }
    if (!primed || lastEmittedStateText_ != state) {
        lastEmittedStateText_ = state;
        emit sessionStateTextChanged();
    }
    if (!primed || lastEmittedGroupVolume_ != volume) {
        lastEmittedGroupVolume_ = volume;
        emit groupVolumeChanged();
    }
    if (!primed || lastEmittedSourceId_ != source) {
        lastEmittedSourceId_ = source;
        emit currentSourceIdChanged();
    }
    if (!primed || lastEmittedMuted_ != muted) {
        lastEmittedMuted_ = muted;
        emit currentMutedChanged();
    }
}

void SessionManager::touchUpdated(AuralisSession& session)
{
    session.updatedAt = QDateTime::currentDateTimeUtc();
}

SessionCommandResult SessionManager::persistAll(const QString& sessionIdForError)
{
    if (persistence_ == nullptr) {
        return SessionCommandResult::PersistenceFailure;
    }
    SessionPersistenceDocument document;
    document.sessions = sessions_;
    QString error;
    const bool ok = persistence_->save(document, &error);
    if (!ok) {
        persistenceDirty_ = true;
        qCWarning(auralisSession) << "SessionPersistenceFailed"
                                  << "session=" << sessionIdForError << "path=" << persistence_->filePath()
                                  << "reason=" << error;
        if (!sessionIdForError.isEmpty()) {
            emit sessionError(sessionIdForError, SessionError::PersistenceFailure, error);
        }
        return SessionCommandResult::PersistenceFailure;
    }
    persistenceDirty_ = false;
    return SessionCommandResult::Accepted;
}

void SessionManager::loadSessions()
{
    if (persistence_ == nullptr) {
        return;
    }
    QString error;
    const SessionPersistenceDocument document = persistence_->load(&error);
    if (!error.isEmpty()) {
        qCWarning(auralisSession) << "SessionLoadWarning" << error;
    }
    sessions_ = document.sessions;
    for (AuralisSession& session : sessions_) {
        session.state = SessionState::Idle;
        for (SessionDevice& device : session.devices) {
            device.runtime = {};
            device.runtime.autoRestoreAllowed = true;
        }
    }
}

SessionIntent SessionManager::currentIntent(const AuralisSession& session) const
{
    switch (session.state) {
    case SessionState::Starting:
        return SessionIntent::Starting;
    case SessionState::Stopping:
        return SessionIntent::Stopping;
    case SessionState::Recovering:
        return SessionIntent::Recovering;
    case SessionState::Failed:
        return SessionIntent::None;
    default:
        break;
    }
    if (session.id == activeSessionId_ && allowsRouteCreation(session.state)) {
        for (const SessionDevice& device : session.devices) {
            if (device.enabled && device.runtime.recovering) {
                return SessionIntent::Recovering;
            }
        }
        if (routing_ != nullptr && !routing_->sourceAvailable(session) && policyAllowsAutoRouteRestore(session)) {
            return SessionIntent::Recovering;
        }
    }
    return SessionIntent::None;
}

void SessionManager::recomputeSession(AuralisSession& session)
{
    if (routing_ != nullptr) {
        routing_->refreshRuntime(session);
    }
    const bool sourceAvailable = routing_ != nullptr && routing_->sourceAvailable(session);
    const SessionHealthSnapshot health = healthSnapshotFromSession(session, sourceAvailable);
    const SessionState oldState = session.state;
    const SessionState newState = SessionStateMachine::recompute(session.state, currentIntent(session), health);
    if (oldState != newState) {
        session.state = newState;
        qCInfo(auralisSession) << "SessionState session=" << session.id << toString(oldState) << "->" << toString(newState);
        emit sessionStateChanged(session.id, oldState, newState);
        if (newState == SessionState::Idle && activeSessionId_ == session.id) {
            activeSessionId_.clear();
        }
        if (newState == SessionState::Active
            || (newState == SessionState::Degraded && oldState == SessionState::Starting)) {
            session.lastUsedAt = QDateTime::currentDateTimeUtc();
            persistAll(session.id);
        }
        if (newState == SessionState::Failed) {
            if (session.error.category != SessionError::RecoveryExhausted) {
                if (session.error.category == SessionError::None) {
                    session.error = {SessionError::RouteActivationFailed, QStringLiteral("Session failed")};
                }
                emit sessionError(session.id, session.error.category, session.error.detail);
            }
        }
        emit sessionUpdated(session.id);
        emitSessionUiSignals();
    }
}

void SessionManager::reconcileActiveSession(AuralisSession& session)
{
    if (generations_.value(session.id) != session.operationGeneration) {
        return;
    }
    if (reconciling_) {
        reconcilePending_ = true;
        return;
    }
    reconciling_ = true;
    reconcilePending_ = false;

    if (session.state == SessionState::Stopping || session.state == SessionState::Idle
        || session.state == SessionState::Failed) {
        reconciling_ = false;
        reconcilePending_ = false;
        return;
    }

    if (routing_ != nullptr) {
        routing_->refreshRuntime(session);
    }

    applyAutoRestoreFlags(session);

    const bool sourceAvailableBefore = routing_ != nullptr && routing_->sourceAvailable(session);
    const ReconcileMode mode = sourceAvailableBefore ? ReconcileMode::Full : ReconcileMode::SuppressCreate;

    if (routing_ != nullptr) {
        routing_->reconcile(session, session.operationGeneration, generations_, mode);
        if (volume_ != nullptr && sourceAvailableBefore) {
            volume_->applySessionVolumes(session);
        }
    }

    const bool sourceAvailable = routing_ != nullptr && routing_->sourceAvailable(session);
    if (!sourceAvailable && session.state != SessionState::Starting && policyAllowsAutoRouteRestore(session)) {
        for (SessionDevice& device : session.devices) {
            if (device.enabled) {
                device.runtime.recovering = true;
            }
        }
    }

    for (SessionDevice& device : session.devices) {
        if (!device.enabled) {
            device.runtime.recovering = false;
            continue;
        }

        if (device.runtime.routeActive) {
            cancelRecovery(session.id, device.deviceId);
            device.runtime.recovering = false;
            device.runtime.lastError = {};
            continue;
        }

        const bool hadRoute = device.runtime.routeRequested;
        const bool missing = !device.runtime.routeActive;
        if (!missing) {
            continue;
        }

        if (session.state == SessionState::Starting) {
            device.runtime.recovering = false;
            continue;
        }

        if (session.recoveryPolicy == RecoveryPolicy::None) {
            if (hadRoute || session.state != SessionState::Starting) {
                device.runtime.autoRestoreAllowed = false;
            }
            device.runtime.recovering = false;
            continue;
        }

        if (!hadRoute && session.state != SessionState::Starting && !device.runtime.endpointAvailable) {
            // Member never had a route and still unavailable — degraded, not recovering yet.
            device.runtime.recovering = false;
            continue;
        }

        if (device.runtime.lastError.category == SessionError::RecoveryExhausted) {
            device.runtime.recovering = false;
            continue;
        }

        device.runtime.recovering = true;
        if (policyAllowsBluetoothReconnect(session) && !device.runtime.connected) {
            requestManagedReconnect(session, device);
        }
    }

    clearStaleMemberErrors(session);
    recomputeSession(session);
    emit sessionUpdated(session.id);

    reconciling_ = false;
    if (reconcilePending_ && generations_.value(session.id) == session.operationGeneration
        && session.state != SessionState::Failed && session.state != SessionState::Idle) {
        reconcileActiveSession(session);
    }
}

void SessionManager::stopSession(AuralisSession& session, bool persist)
{
    bumpGeneration(session);
    const SessionState oldState = session.state;
    session.state = SessionState::Stopping;
    removeReconnectSuppressions(session);
    cancelAllRecovery(session);
    for (SessionDevice& device : session.devices) {
        device.runtime.recovering = false;
        device.runtime.managedReconnectRequested = false;
    }
    if (routing_ != nullptr) {
        routing_->stopSessionRoutes(session);
        routing_->refreshRuntime(session);
    }
    session.state = SessionState::Idle;
    if (activeSessionId_ == session.id) {
        activeSessionId_.clear();
    }
    if (oldState != SessionState::Idle) {
        emit sessionStateChanged(session.id, oldState, SessionState::Idle);
    }
    if (persist) {
        persistAll(session.id);
    }
    emit sessionUpdated(session.id);
    emitSessionUiSignals();
}

void SessionManager::bumpGeneration(AuralisSession& session)
{
    const quint64 generation = ++nextGeneration_;
    generations_[session.id] = generation;
    session.operationGeneration = generation;
}

QString SessionManager::findActiveSessionId() const
{
    for (const AuralisSession& session : sessions_) {
        if (isActiveLifecycleState(session.state) || session.state == SessionState::Failed) {
            return session.id;
        }
    }
    return activeSessionId_;
}

bool SessionManager::isActiveLifecycleState(SessionState state) const
{
    switch (state) {
    case SessionState::Starting:
    case SessionState::Active:
    case SessionState::Degraded:
    case SessionState::Recovering:
    case SessionState::Stopping:
        return true;
    default:
        return false;
    }
}

bool SessionManager::allowsRouteCreation(SessionState state) const
{
    switch (state) {
    case SessionState::Starting:
    case SessionState::Active:
    case SessionState::Degraded:
    case SessionState::Recovering:
        return true;
    default:
        return false;
    }
}

bool SessionManager::policyAllowsAutoRouteRestore(const AuralisSession& session) const
{
    return session.recoveryPolicy == RecoveryPolicy::RestoreRoutesOnly
        || session.recoveryPolicy == RecoveryPolicy::ReconnectAndRestore;
}

bool SessionManager::policyAllowsBluetoothReconnect(const AuralisSession& session) const
{
    return session.autoReconnect && session.recoveryPolicy == RecoveryPolicy::ReconnectAndRestore;
}

void SessionManager::applyAutoRestoreFlags(AuralisSession& session)
{
    if (session.state == SessionState::Starting) {
        for (SessionDevice& device : session.devices) {
            if (device.enabled) {
                device.runtime.autoRestoreAllowed = true;
            }
        }
        return;
    }
    if (session.recoveryPolicy == RecoveryPolicy::None) {
        for (SessionDevice& device : session.devices) {
            if (!device.enabled || !device.runtime.routeActive) {
                device.runtime.autoRestoreAllowed = false;
            }
        }
        return;
    }
    for (SessionDevice& device : session.devices) {
        if (device.enabled) {
            device.runtime.autoRestoreAllowed = true;
        }
    }
}

void SessionManager::requestManagedReconnect(AuralisSession& session, SessionDevice& device)
{
    device.runtime.recovering = true;
    if (!policyAllowsBluetoothReconnect(session)) {
        return;
    }
    if (!device.enabled) {
        return;
    }
    if (device.runtime.managedReconnectRequested) {
        return;
    }
    const QString path = devicePathForAddress(device.deviceId);
    if (path.isEmpty()) {
        return;
    }
    if (deviceRegistry_ != nullptr) {
        const auto* devData = deviceRegistry_->findByObjectPath(path);
        if (devData != nullptr && !devData->autoReconnectEnabled) {
            return;
        }
    }
    device.runtime.managedReconnectRequested = true;
    managedReconnectRequestsForTest_.push_back(path);
    if (managedReconnectHook_) {
        managedReconnectHook_(path);
        return;
    }
    if (bluetooth_ != nullptr) {
        bluetooth_->requestManagedReconnect(path);
    }
}

void SessionManager::cancelRecovery(const QString& sessionId, const QString& deviceId)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return;
    }
    for (SessionDevice& device : session->devices) {
        if (device.deviceId != deviceId) {
            continue;
        }
        if (device.runtime.managedReconnectRequested) {
            const QString path = devicePathForAddress(device.deviceId);
            if (!path.isEmpty() && bluetooth_ != nullptr) {
                bluetooth_->cancelManagedReconnect(path);
            }
            device.runtime.managedReconnectRequested = false;
        }
        device.runtime.recovering = false;
        return;
    }
}

void SessionManager::cancelAllRecovery(AuralisSession& session)
{
    for (SessionDevice& device : session.devices) {
        cancelRecovery(session.id, device.deviceId);
    }
}

void SessionManager::installReconnectSuppressions(const AuralisSession& session)
{
    if (bluetooth_ == nullptr) {
        return;
    }
    for (const SessionDevice& device : session.devices) {
        if (!device.enabled) {
            continue;
        }
        const QString path = devicePathForAddress(device.deviceId);
        if (!path.isEmpty()) {
            bluetooth_->suppressAutoReconnect(path);
        }
    }
}

void SessionManager::removeReconnectSuppressions(const AuralisSession& session)
{
    if (bluetooth_ == nullptr) {
        return;
    }
    for (const SessionDevice& device : session.devices) {
        const QString path = devicePathForAddress(device.deviceId);
        if (!path.isEmpty()) {
            bluetooth_->unsuppressAutoReconnect(path);
        }
    }
}

void SessionManager::clearStaleMemberErrors(AuralisSession& session)
{
    for (SessionDevice& device : session.devices) {
        if (device.runtime.routeActive && device.runtime.lastError.hasError()) {
            device.runtime.lastError = {};
        }
    }
    if (session.state == SessionState::Active && session.error.hasError()) {
        session.error = {};
    }
}

QString SessionManager::canonicalMemberDeviceId(const QString& deviceId) const
{
    const QString trimmed = deviceId.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    if (deviceRegistry_ != nullptr) {
        if (const auto* byPath = deviceRegistry_->findByObjectPath(trimmed)) {
            if (!byPath->address.trimmed().isEmpty()) {
                return byPath->address.trimmed().toUpper();
            }
        }
        const QString upperPath = trimmed.toUpper();
        for (const auralis::bluetooth::BluetoothDeviceData& device : deviceRegistry_->devices()) {
            if (device.objectPath.toUpper() == upperPath && !device.address.trimmed().isEmpty()) {
                return device.address.trimmed().toUpper();
            }
        }
    }
    if (const auto normalized = auralis::bluetooth::normalizeBluetoothAddress(trimmed)) {
        return *normalized;
    }
    return {};
}

QString SessionManager::devicePathForAddress(const QString& address) const
{
    if (deviceRegistry_ == nullptr) {
        return {};
    }
    const QString normalized = address.trimmed().toUpper();
    for (const auralis::bluetooth::BluetoothDeviceData& device : deviceRegistry_->devices()) {
        if (device.address.trimmed().toUpper() == normalized) {
            return device.objectPath;
        }
    }
    return {};
}

SessionCommandResult SessionManager::validateVolume(double value) const
{
    if (!std::isfinite(value)) {
        return SessionCommandResult::InvalidArgument;
    }
    return SessionCommandResult::Accepted;
}

} // namespace auralis::session

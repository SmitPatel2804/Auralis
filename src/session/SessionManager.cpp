#include <auralis/session/SessionManager.h>

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/LoggingCategories.h>
#include <auralis/session/SessionStateMachine.h>

#include <QStandardPaths>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace auralis::session {
namespace {

double clampVolume(double value)
{
    if (!std::isfinite(value)) {
        return 0.0;
    }
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
}

SessionManager::SessionManager(
    auralis::bluetooth::BluetoothManager* bluetooth,
    auralis::audio::PipeWireManager* pipeWire,
    const QString& persistencePath,
    QObject* parent)
    : QObject(parent)
    , bluetooth_(bluetooth)
    , pipeWire_(pipeWire)
    , persistence_(std::make_unique<SessionPersistence>(
          persistencePath.isEmpty() ? defaultPersistencePath() : persistencePath))
{
    if (pipeWire_ != nullptr) {
        router_ = pipeWire_->audioRouter();
        endpoints_ = pipeWire_->endpointRegistry();
    }
    if (bluetooth_ != nullptr) {
        deviceRegistry_ = bluetooth_->deviceRegistry();
    }
}

SessionManager::SessionManager(
    auralis::audio::AudioRouter* router,
    auralis::audio::AudioEndpointRegistry* endpoints,
    auralis::bluetooth::DeviceRegistry* devices,
    auralis::bluetooth::BluetoothManager* bluetooth,
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

    if (pipeWire_ != nullptr) {
        router_ = pipeWire_->audioRouter();
        endpoints_ = pipeWire_->endpointRegistry();
    }
    if (bluetooth_ != nullptr) {
        deviceRegistry_ = bluetooth_->deviceRegistry();
    }

    if (router_ != nullptr) {
        routing_ = std::make_unique<RoutingCoordinator>(router_, endpoints_, deviceRegistry_);
        volume_ = std::make_unique<VolumeCoordinator>(router_);
        connect(router_, &auralis::audio::AudioRouter::routeStateChanged, this, &SessionManager::handleRouteStateChanged);
        connect(router_, &auralis::audio::AudioRouter::routeRemoved, this, &SessionManager::handleRouteRemoved);
    }
    if (pipeWire_ != nullptr) {
        connect(pipeWire_, &auralis::audio::PipeWireManager::graphRevisionChanged, this, &SessionManager::handleExternalGraphChanged);
    }
    if (deviceRegistry_ != nullptr) {
        connect(deviceRegistry_, &auralis::bluetooth::DeviceRegistry::deviceUpdated, this, &SessionManager::handleDeviceRegistryChanged);
        connect(deviceRegistry_, &auralis::bluetooth::DeviceRegistry::deviceRemoved, this, &SessionManager::handleDeviceRegistryChanged);
        connect(deviceRegistry_, &auralis::bluetooth::DeviceRegistry::deviceAdded, this, &SessionManager::handleDeviceRegistryChanged);
    }

    recoverySweep_.setInterval(1000);
    connect(&recoverySweep_, &QTimer::timeout, this, &SessionManager::handleRecoveryTick);

    loadSessions();
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
    const QList<QString> timerKeys = recoveryTimers_.keys();
    for (const QString& key : timerKeys) {
        if (QTimer* timer = recoveryTimers_.take(key)) {
            timer->stop();
            timer->deleteLater();
        }
    }

    for (AuralisSession& session : sessions_) {
        if (isActiveLifecycleState(session.state)) {
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
        return toString(SessionState::Idle);
    }
    for (const AuralisSession& session : sessions_) {
        if (session.id == activeSessionId_) {
            return toString(session.state);
        }
    }
    return toString(SessionState::Idle);
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

QString SessionManager::createSession(const QString& name)
{
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
    persistAll();
    qCInfo(auralisSession) << "SessionCreated id=" << session.id << "name=" << session.name;
    emit sessionAdded(session.id);
    emit sessionsChanged();
    emitSessionUiSignals();
    return session.id;
}

SessionCommandResult SessionManager::deleteSession(const QString& sessionId)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    if (isActiveLifecycleState(session->state)) {
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
    persistAll();
    emit sessionRemoved(sessionId);
    emit sessionsChanged();
    emitSessionUiSignals();
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::renameSession(const QString& sessionId, const QString& name)
{
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
    persistAll();
    emit sessionUpdated(sessionId);
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::addDevice(const QString& sessionId, const QString& deviceId, const QString& role)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    const QString normalized = deviceId.trimmed().toUpper();
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
    session->devices.push_back(device);
    touchUpdated(*session);
    persistAll();
    emit sessionUpdated(sessionId);
    if (sessionId == activeSessionId_ && isActiveLifecycleState(session->state)) {
        reconcileActiveSession(*session);
    }
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::removeDevice(const QString& sessionId, const QString& deviceId)
{
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
        if (!device.runtime.routeId.isEmpty() && router_ != nullptr) {
            router_->deactivateRoute(device.runtime.routeId);
            router_->removeRoute(device.runtime.routeId);
            device.runtime.routeId.clear();
            device.runtime.routeActive = false;
        }
        session->devices.removeAt(i);
        touchUpdated(*session);
        persistAll();
        emit sessionUpdated(sessionId);
        if (sessionId == activeSessionId_) {
            reconcileActiveSession(*session);
        }
        return SessionCommandResult::Accepted;
    }
    return SessionCommandResult::MemberNotFound;
}

SessionCommandResult SessionManager::setDeviceEnabled(const QString& sessionId, const QString& deviceId, bool enabled)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    for (SessionDevice& device : session->devices) {
        if (device.deviceId == deviceId.trimmed().toUpper()) {
            device.enabled = enabled;
            touchUpdated(*session);
            persistAll();
            emit sessionUpdated(sessionId);
            if (sessionId == activeSessionId_ && isActiveLifecycleState(session->state)) {
                reconcileActiveSession(*session);
            }
            return SessionCommandResult::Accepted;
        }
    }
    return SessionCommandResult::MemberNotFound;
}

SessionCommandResult SessionManager::setDeviceRole(const QString& sessionId, const QString& deviceId, const QString& role)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    for (SessionDevice& device : session->devices) {
        if (device.deviceId == deviceId.trimmed().toUpper()) {
            bool ok = false;
            device.role = sessionDeviceRoleFromString(role, &ok);
            if (!ok && !role.trimmed().isEmpty()) {
                return SessionCommandResult::InternalError;
            }
            touchUpdated(*session);
            persistAll();
            emit sessionUpdated(sessionId);
            return SessionCommandResult::Accepted;
        }
    }
    return SessionCommandResult::MemberNotFound;
}

SessionCommandResult SessionManager::setSource(const QString& sessionId, const QString& sourceId)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    session->sourceId = sourceId.trimmed();
    touchUpdated(*session);
    persistAll();
    emit sessionUpdated(sessionId);
    if (sessionId == activeSessionId_ && isActiveLifecycleState(session->state)) {
        if (routing_ != nullptr) {
            routing_->stopSessionRoutes(*session);
        }
        reconcileActiveSession(*session);
    }
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::activateSession(const QString& sessionId)
{
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
    if (isActiveLifecycleState(session->state) && sessionId == activeSessionId_) {
        return SessionCommandResult::Accepted;
    }

    const quint64 generation = ++nextGeneration_;
    generations_[sessionId] = generation;
    session->operationGeneration = generation;
    activeSessionId_ = sessionId;
    session->state = SessionState::Starting;
    session->lastUsedAt = QDateTime::currentDateTimeUtc();
    session->restoreIntent = false;
    touchUpdated(*session);
    persistAll();
    emit sessionStateChanged(sessionId, SessionState::Idle, SessionState::Starting);
    emit sessionUpdated(sessionId);
    emitSessionUiSignals();

    if (!recoverySweep_.isActive()) {
        recoverySweep_.start();
    }
    reconcileActiveSession(*session);
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::deactivateSession(const QString& sessionId)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    if (!isActiveLifecycleState(session->state) && session->state != SessionState::Failed) {
        return SessionCommandResult::Accepted;
    }
    ++nextGeneration_;
    generations_[sessionId] = nextGeneration_;
    stopSession(*session, true);
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::retrySession(const QString& sessionId)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    if (session->state != SessionState::Failed && session->state != SessionState::Degraded) {
        return SessionCommandResult::InvalidState;
    }
    return activateSession(sessionId);
}

SessionCommandResult SessionManager::setGroupVolume(const QString& sessionId, double value)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    session->groupVolume = clampVolume(value);
    touchUpdated(*session);
    persistAll();
    if (volume_ != nullptr) {
        volume_->applySessionVolumes(*session);
    }
    emit sessionUpdated(sessionId);
    emitSessionUiSignals();
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::setSessionMuted(const QString& sessionId, bool muted)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    session->muted = muted;
    touchUpdated(*session);
    persistAll();
    if (volume_ != nullptr) {
        volume_->applySessionVolumes(*session);
    }
    emit sessionUpdated(sessionId);
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::setDeviceVolume(const QString& sessionId, const QString& deviceId, double value)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    for (SessionDevice& device : session->devices) {
        if (device.deviceId == deviceId.trimmed().toUpper()) {
            device.volumeTrim = clampVolume(value);
            touchUpdated(*session);
            persistAll();
            if (volume_ != nullptr) {
                volume_->applyMemberVolume(*session, device);
            }
            emit sessionUpdated(sessionId);
            return SessionCommandResult::Accepted;
        }
    }
    return SessionCommandResult::MemberNotFound;
}

SessionCommandResult SessionManager::setDeviceMuted(const QString& sessionId, const QString& deviceId, bool muted)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    for (SessionDevice& device : session->devices) {
        if (device.deviceId == deviceId.trimmed().toUpper()) {
            device.muted = muted;
            touchUpdated(*session);
            persistAll();
            if (volume_ != nullptr) {
                volume_->applyMemberVolume(*session, device);
            }
            emit sessionUpdated(sessionId);
            return SessionCommandResult::Accepted;
        }
    }
    return SessionCommandResult::MemberNotFound;
}

SessionCommandResult SessionManager::setAutoReconnect(const QString& sessionId, bool enabled)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    session->autoReconnect = enabled;
    touchUpdated(*session);
    persistAll();
    emit sessionUpdated(sessionId);
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::setRecoveryPolicy(const QString& sessionId, const QString& policy)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    bool ok = false;
    session->recoveryPolicy = recoveryPolicyFromString(policy, &ok);
    if (!ok) {
        return SessionCommandResult::InternalError;
    }
    touchUpdated(*session);
    persistAll();
    emit sessionUpdated(sessionId);
    return SessionCommandResult::Accepted;
}

SessionCommandResult SessionManager::restoreLastSession()
{
    QString candidateId;
    QDateTime latest;
    for (const AuralisSession& session : sessions_) {
        if (!session.restoreIntent || !session.lastUsedAt.isValid()) {
            continue;
        }
        if (!candidateId.isEmpty() && session.lastUsedAt <= latest) {
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
            if (!candidateId.isEmpty() && session.lastUsedAt <= latest) {
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
        reconcileActiveSession(*session);
    }
}

void SessionManager::handleRouteStateChanged(const QString& routeId, auralis::audio::RouteState state)
{
    Q_UNUSED(routeId);
    if (activeSessionId_.isEmpty()) {
        return;
    }
    switch (state) {
    case auralis::audio::RouteState::Planning:
    case auralis::audio::RouteState::Ready:
    case auralis::audio::RouteState::Activating:
    case auralis::audio::RouteState::Deactivating:
        return;
    default:
        break;
    }
    if (AuralisSession* session = mutableSession(activeSessionId_)) {
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
        reconcileActiveSession(*session);
    }
}

void SessionManager::handleDeviceRegistryChanged()
{
    if (activeSessionId_.isEmpty()) {
        return;
    }
    if (AuralisSession* session = mutableSession(activeSessionId_)) {
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
        reconcileActiveSession(*session);
    }
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
    emit currentSessionIdChanged();
    emit sessionStateTextChanged();
    emit groupVolumeChanged();
}

void SessionManager::touchUpdated(AuralisSession& session)
{
    session.updatedAt = QDateTime::currentDateTimeUtc();
}

bool SessionManager::persistAll()
{
    if (persistence_ == nullptr) {
        return false;
    }
    SessionPersistenceDocument document;
    document.sessions = sessions_;
    QString error;
    const bool ok = persistence_->save(document, &error);
    if (!ok) {
        qCWarning(auralisSession) << "SessionPersistFailed" << error;
    }
    return ok;
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
    default:
        break;
    }
    if (session.id == activeSessionId_ && isActiveLifecycleState(session.state)) {
        for (const SessionDevice& device : session.devices) {
            if (device.enabled && device.runtime.recovering) {
                return SessionIntent::Recovering;
            }
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
        if (newState == SessionState::Active) {
            session.lastUsedAt = QDateTime::currentDateTimeUtc();
            persistAll();
        }
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

    if (session.state == SessionState::Stopping) {
        if (routing_ != nullptr) {
            routing_->stopSessionRoutes(session);
            routing_->refreshRuntime(session);
        }
        recomputeSession(session);
        if (session.state == SessionState::Idle) {
            persistAll();
            emit sessionUpdated(session.id);
        }
        reconciling_ = false;
        if (reconcilePending_) {
            reconcileActiveSession(session);
        }
        return;
    }

    if (routing_ != nullptr) {
        routing_->reconcile(session, session.operationGeneration, generations_);
        if (volume_ != nullptr) {
            volume_->applySessionVolumes(session);
        }
    }

    for (SessionDevice& device : session.devices) {
        if (!device.enabled) {
            device.runtime.recovering = false;
            continue;
        }
        const bool needsRecovery = !device.runtime.routeActive
            && (session.autoReconnect || session.recoveryPolicy != RecoveryPolicy::None);
        const bool hadRoute = device.runtime.routeRequested;
        if (needsRecovery && hadRoute && session.recoveryPolicy == RecoveryPolicy::ReconnectAndRestore && !device.runtime.connected) {
            scheduleRecovery(session, device);
        } else if (needsRecovery && hadRoute && device.runtime.connected && routing_ != nullptr && !routing_->memberEndpointAvailable(device.deviceId)) {
            device.runtime.recovering = true;
        } else if (device.runtime.routeActive) {
            cancelRecovery(session.id, device.deviceId);
            device.runtime.recovering = false;
        } else if (needsRecovery && hadRoute) {
            device.runtime.recovering = session.recoveryPolicy != RecoveryPolicy::None;
        } else {
            device.runtime.recovering = false;
        }
    }

    recomputeSession(session);
    emit sessionUpdated(session.id);

    reconciling_ = false;
    if (reconcilePending_) {
        reconcileActiveSession(session);
    }
}

void SessionManager::stopSession(AuralisSession& session, bool persist)
{
    const SessionState oldState = session.state;
    session.state = SessionState::Stopping;
    for (SessionDevice& device : session.devices) {
        cancelRecovery(session.id, device.deviceId);
        device.runtime.recovering = false;
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
        persistAll();
    }
    emit sessionUpdated(session.id);
    emitSessionUiSignals();
}

QString SessionManager::findActiveSessionId() const
{
    for (const AuralisSession& session : sessions_) {
        if (isActiveLifecycleState(session.state)) {
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

void SessionManager::scheduleRecovery(AuralisSession& session, SessionDevice& device)
{
    device.runtime.recovering = true;
    if (bluetooth_ == nullptr || !session.autoReconnect) {
        return;
    }
    const QString path = devicePathForAddress(device.deviceId);
    if (path.isEmpty()) {
        return;
    }
    const QString key = session.id + QLatin1Char(':') + device.deviceId;
    if (!recoveryTimers_.contains(key)) {
        auto* timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, sessionId = session.id, deviceId = device.deviceId]() {
            if (generations_.value(sessionId) == 0) {
                return;
            }
            if (AuralisSession* active = mutableSession(sessionId)) {
                if (active->state == SessionState::Stopping || active->state == SessionState::Idle) {
                    return;
                }
                const QString devicePath = devicePathForAddress(deviceId);
                if (!devicePath.isEmpty() && bluetooth_ != nullptr) {
                    bluetooth_->reconnectDevice(devicePath);
                }
                reconcileActiveSession(*active);
            }
        });
        recoveryTimers_.insert(key, timer);
    }
    if (QTimer* timer = recoveryTimers_.value(key)) {
        timer->start(500);
    }
}

void SessionManager::cancelRecovery(const QString& sessionId, const QString& deviceId)
{
    const QString key = sessionId + QLatin1Char(':') + deviceId;
    if (QTimer* timer = recoveryTimers_.take(key)) {
        timer->stop();
        timer->deleteLater();
    }
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

SessionCommandResult SessionManager::validateCrudSession(const QString& sessionId, AuralisSession** out)
{
    AuralisSession* session = mutableSession(sessionId);
    if (session == nullptr) {
        return SessionCommandResult::SessionNotFound;
    }
    if (out != nullptr) {
        *out = session;
    }
    return SessionCommandResult::Accepted;
}

} // namespace auralis::session

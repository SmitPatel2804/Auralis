#include <auralis/audio/AudioRouter.h>

#include <auralis/audio/AudioSourceListModel.h>
#include <auralis/audio/PipeWireVirtualOutput.h>
#include <auralis/audio/RouteListModel.h>
#include <auralis/core/LoggingCategories.h>

#include <QDateTime>
#include <QHash>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace auralis::audio {
namespace {

QStringList uniqueIds(const QStringList& ids)
{
    QStringList unique;
    for (const QString& id : ids) {
        if (!id.isEmpty() && !unique.contains(id)) {
            unique.push_back(id);
        }
    }
    return unique;
}

bool hasLiveOwnership(const AudioRoute& route)
{
    for (const OwnedLink& owned : route.ownedLinks) {
        if (owned.ownershipToken != 0) {
            return true;
        }
    }
    return false;
}

struct DelayBridgeNodes {
    quint32 captureNodeId = 0;
    quint32 playbackNodeId = 0;
    bool ready = false;
};

const PipeWirePortInfo* findNodeAudioPort(
    const PipeWireObjectStore& store,
    quint32 nodeId,
    PipeWirePortDirection direction,
    const QString& channel)
{
    const PipeWirePortInfo* fallback = nullptr;
    for (const PipeWirePortInfo& port : store.portsForNode(nodeId)) {
        if (port.direction != direction || port.control || port.monitor) {
            continue;
        }
        if (!channel.isEmpty() && port.audioChannel == channel) {
            return &port;
        }
        if (fallback == nullptr) {
            fallback = &port;
        }
    }
    return fallback;
}

DelayBridgeNodes findDelayBridge(const PipeWireObjectStore& store, const QString& endpointId)
{
    DelayBridgeNodes result;
    for (const PipeWireNodeInfo& node : store.nodes()) {
        if (node.properties.value(QStringLiteral("auralis.delay.endpoint")) != endpointId) {
            continue;
        }
        const QString role = node.properties.value(QStringLiteral("auralis.delay.role"));
        if (role == QLatin1String("capture") || node.mediaClass == QLatin1String("Audio/Sink")) {
            result.captureNodeId = node.globalId;
        } else if (role == QLatin1String("playback")
                   || node.mediaClass.startsWith(QLatin1String("Stream/Output"))) {
            result.playbackNodeId = node.globalId;
        }
    }
    if (result.captureNodeId == 0 || result.playbackNodeId == 0) {
        return result;
    }
    const bool captureReady =
        findNodeAudioPort(store, result.captureNodeId, PipeWirePortDirection::Input, {}) != nullptr;
    const bool playbackReady =
        findNodeAudioPort(store, result.playbackNodeId, PipeWirePortDirection::Output, {}) != nullptr;
    result.ready = captureReady && playbackReady;
    return result;
}

} // namespace

AudioRouter::AudioRouter(
    PipeWireObjectStore* store,
    AudioEndpointRegistry* endpoints,
    IPipeWireLinkBackend* backend,
    QObject* parent)
    : QObject(parent)
    , store_(store)
    , endpoints_(endpoints)
    , backend_(backend)
    , links_(backend)
    , volume_(backend)
    , sourceModel_(new AudioSourceListModel(this))
    , qmlRouteStateText_(toString(RouteState::Inactive))
{
    routeModel_ = new RouteListModel(this, this);
}

AudioRouter::~AudioRouter()
{
    stopAllActivationTimeouts();
}

QAbstractItemModel* AudioRouter::sources() const
{
    return sourceModel_;
}

QAbstractItemModel* AudioRouter::routeModel() const
{
    return routeModel_;
}

int AudioRouter::sourceCount() const
{
    return sourceModel_ != nullptr ? sourceModel_->rowCount() : 0;
}

QString AudioRouter::sourceDisplayName(const QString& sourceId) const
{
    for (const AudioSource& source : sources_) {
        if (source.id == sourceId) {
            return sourceListDisplayName(source);
        }
    }
    return sourceId;
}

QString AudioRouter::selectableSourceIdAt(int row) const
{
    return sourceModel_ != nullptr ? sourceModel_->sourceIdAt(row) : QString();
}

const AudioRoute* AudioRouter::plannerRoute() const
{
    for (const AudioRoute& route : routes_) {
        if (route.ownerType == RouteOwnerType::Manual) {
            return &route;
        }
    }
    return nullptr;
}

QString AudioRouter::currentRouteId() const
{
    const AudioRoute* route = plannerRoute();
    return route == nullptr ? QString() : route->id;
}

QString AudioRouter::routeStateText() const
{
    const AudioRoute* route = plannerRoute();
    if (route == nullptr) {
        return toString(RouteState::Inactive);
    }
    return toString(route->state);
}

QString AudioRouter::lastErrorText() const
{
    const AudioRoute* route = plannerRoute();
    if (route == nullptr || !route->error.hasError()) {
        return {};
    }
    return QStringLiteral("%1: %2").arg(toString(route->error.category), route->error.detail);
}

bool AudioRouter::routeEnabled() const
{
    const AudioRoute* route = plannerRoute();
    return route != nullptr && route->enabled;
}

double AudioRouter::routeVolume() const
{
    const AudioRoute* route = plannerRoute();
    return route == nullptr ? 1.0 : route->volume;
}

bool AudioRouter::routeMuted() const
{
    const AudioRoute* route = plannerRoute();
    return route != nullptr && route->muted;
}

bool AudioRouter::volumeCapable() const
{
    const AudioRoute* route = plannerRoute();
    if (route == nullptr || endpoints_ == nullptr) {
        return false;
    }
    for (const QString& destId : route->destinationIds) {
        const AudioEndpoint* endpoint = endpoints_->findById(destId);
        if (endpoint == nullptr || !volume_.volumeSupported(endpoint->pipeWireObjectId)) {
            return false;
        }
    }
    return !route->destinationIds.isEmpty();
}

int AudioRouter::ownedLinkCount() const
{
    int count = 0;
    for (const AudioRoute& route : routes_) {
        count += static_cast<int>(route.ownedLinks.size());
    }
    return count;
}

void AudioRouter::setActivationTimeoutMs(int milliseconds)
{
    activationTimeoutMs_ = std::max(1, milliseconds);
}

int AudioRouter::activationTimeoutMs() const noexcept
{
    return activationTimeoutMs_;
}

void AudioRouter::setDelayGraphCommitMs(int milliseconds)
{
    delayGraphCommitMs_ = std::max(0, milliseconds);
}

int AudioRouter::delayGraphCommitMs() const noexcept
{
    return delayGraphCommitMs_;
}

QVector<AudioSource> AudioRouter::sourceList() const
{
    return sources_;
}

QVector<AudioRoute> AudioRouter::routes() const
{
    return routes_;
}

std::optional<AudioRoute> AudioRouter::routeById(const QString& id) const
{
    for (const AudioRoute& route : routes_) {
        if (route.id == id) {
            return route;
        }
    }
    return std::nullopt;
}

AudioRoute* AudioRouter::mutableRoute(const QString& id)
{
    for (AudioRoute& route : routes_) {
        if (route.id == id) {
            return &route;
        }
    }
    return nullptr;
}

QString AudioRouter::createRoute(const QString& sourceId, const QStringList& destinationEndpointIds)
{
    return createRouteInternal(sourceId, destinationEndpointIds, RouteOwnerType::Manual, {});
}

QString AudioRouter::createSessionRoute(
    const QString& sessionId,
    const QString& sourceId,
    const QStringList& destinationEndpointIds)
{
    return createRouteInternal(sourceId, destinationEndpointIds, RouteOwnerType::Session, sessionId);
}

QString AudioRouter::createRouteInternal(
    const QString& sourceId,
    const QStringList& destinationEndpointIds,
    RouteOwnerType ownerType,
    const QString& ownerId)
{
    RouteErrorInfo error;
    const QStringList dests = uniqueIds(destinationEndpointIds);
    if (!validateSelection(sourceId, dests, &error)) {
        emit routeError({}, error.category, error.detail);
        emitQmlPropertyNotifications();
        return {};
    }

    AudioRoute route;
    route.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    route.sourceId = sourceId;
    route.destinationIds = dests;
    route.createdAt = QDateTime::currentDateTimeUtc();
    route.state = RouteState::Inactive;
    route.ownerType = ownerType;
    route.ownerId = ownerId;
    routes_.push_back(route);
    qCInfo(auralisAudio) << "AudioRouter RouteCreated id=" << route.id << "source=" << sourceId
                         << "destinations=" << dests.size() << "owner=" << toString(ownerType);
    emit routeAdded(route.id);
    emitRouteSignals(route);
    return route.id;
}

bool AudioRouter::rejectPlannerMutation(const AudioRoute& route)
{
    if (route.ownerType != RouteOwnerType::Session) {
        return false;
    }
    emit routeError(
        route.id,
        RouteError::PermissionDenied,
        QStringLiteral("Session-owned routes cannot be edited from the planner"));
    emitQmlPropertyNotifications();
    return true;
}

void AudioRouter::removeRoute(const QString& routeId)
{
    AudioRoute* route = mutableRoute(routeId);
    if (route == nullptr) {
        return;
    }
    generations_[routeId] = ++nextGeneration_;
    route->enabled = false;
    stopActivationTimeout(routeId);
    replanBackups_.remove(routeId);
    links_.destroyLinks(route->ownedLinks);
    emit routeRemoved(routeId);
    for (int i = 0; i < routes_.size(); ++i) {
        if (routes_.at(i).id == routeId) {
            routes_.removeAt(i);
            break;
        }
    }
    emit routeChanged(routeId);
    emitQmlPropertyNotifications();
}

void AudioRouter::activateRoute(const QString& routeId)
{
    AudioRoute* route = mutableRoute(routeId);
    if (route == nullptr) {
        emit routeError(routeId, RouteError::InternalError, QStringLiteral("Unknown route"));
        emitQmlPropertyNotifications();
        return;
    }
    if (route->state == RouteState::Active || route->state == RouteState::Activating) {
        return;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (route->state == RouteState::Failed
        && nowMs - lastActivateAttemptMs_.value(routeId, 0) < 250) {
        return;
    }
    lastActivateAttemptMs_.insert(routeId, nowMs);
    if (connectionState_ != PipeWireConnectionState::Connected || !initialSyncComplete_) {
        setError(*route, RouteError::PipeWireDisconnected, QStringLiteral("Audio backend is not connected"));
        setState(*route, RouteState::Failed);
        return;
    }
    const quint64 generation = ++nextGeneration_;
    generations_[routeId] = generation;
    beginActivation(*route, generation);
}

void AudioRouter::syncSessionDestinations(const QString& sessionId)
{
    if (sessionId.isEmpty()) {
        return;
    }
    for (AudioRoute& route : routes_) {
        if (route.ownerType != RouteOwnerType::Session || route.ownerId != sessionId || !route.enabled) {
            continue;
        }
        if (tryActivateCompensatedFanout(route)) {
            return;
        }
        applyDelayBridges(sessionGroup(sessionId));
        replanIfEnabled(route);
        return;
    }
}

void AudioRouter::deactivateRoute(const QString& routeId)
{
    AudioRoute* route = mutableRoute(routeId);
    if (route == nullptr) {
        return;
    }
    generations_[routeId] = ++nextGeneration_;
    route->enabled = false;
    stopActivationTimeout(routeId);
    const RouteOwnerType ownerType = route->ownerType;
    const QString ownerId = route->ownerId;
    if (route->state == RouteState::Inactive && route->ownedLinks.isEmpty()) {
        emitQmlPropertyNotifications();
        return;
    }
    setState(*route, RouteState::Deactivating);
    links_.destroyLinks(route->ownedLinks);
    route->ownedLinks.clear();
    route->fanoutRole = RouteFanoutRole::None;
    setState(*route, RouteState::Inactive);
    qCInfo(auralisAudio) << "AudioRouter RouteDeactivated id=" << routeId;
    if (ownerType == RouteOwnerType::Session) {
        releaseFanoutIfUnused(ownerId);
    }
}

void AudioRouter::setRouteSource(const QString& routeId, const QString& sourceId)
{
    AudioRoute* route = mutableRoute(routeId);
    if (route == nullptr) {
        return;
    }
    if (rejectPlannerMutation(*route)) {
        return;
    }
    RouteErrorInfo error;
    if (!validateSelection(sourceId, route->destinationIds, &error)) {
        setError(*route, error.category, error.detail);
        return;
    }
    route->sourceId = sourceId;
    emitRouteSignals(*route);
    if (route->enabled) {
        replanIfEnabled(*route);
    }
}

void AudioRouter::setRouteDestinations(const QString& routeId, const QStringList& destinationEndpointIds)
{
    AudioRoute* route = mutableRoute(routeId);
    if (route == nullptr) {
        return;
    }
    if (rejectPlannerMutation(*route)) {
        return;
    }
    const QStringList dests = uniqueIds(destinationEndpointIds);
    RouteErrorInfo error;
    if (!validateSelection(route->sourceId, dests, &error)) {
        setError(*route, error.category, error.detail);
        return;
    }
    route->destinationIds = dests;
    emitRouteSignals(*route);
    if (route->enabled) {
        replanIfEnabled(*route);
    }
}

void AudioRouter::setDestinationVolume(const QString& endpointId, double value)
{
    if (endpoints_ == nullptr) {
        return;
    }
    const AudioEndpoint* endpoint = endpoints_->findById(endpointId);
    if (endpoint == nullptr) {
        emit routeError(currentRouteId(), RouteError::DestinationNotFound, QStringLiteral("Unknown endpoint"));
        emitQmlPropertyNotifications();
        return;
    }
    const VolumeApplyResult result = volume_.setDestinationVolume(endpoint->pipeWireObjectId, endpointId, value);
    if (!result.allSucceeded) {
        qCWarning(auralisAudio) << "AudioRouter DestinationVolumeFailed endpoint=" << endpointId
                                << "node=" << endpoint->pipeWireObjectId << result.error.detail;
        emit routeError(currentRouteId(), result.error.category, result.error.detail);
        emitQmlPropertyNotifications();
    }
}

void AudioRouter::setDestinationMuted(const QString& endpointId, bool muted)
{
    if (endpoints_ == nullptr) {
        return;
    }
    const AudioEndpoint* endpoint = endpoints_->findById(endpointId);
    if (endpoint == nullptr) {
        emit routeError(currentRouteId(), RouteError::DestinationNotFound, QStringLiteral("Unknown endpoint"));
        emitQmlPropertyNotifications();
        return;
    }
    const VolumeApplyResult result = volume_.setDestinationMuted(endpoint->pipeWireObjectId, endpointId, muted);
    if (!result.allSucceeded) {
        emit routeError(currentRouteId(), result.error.category, result.error.detail);
        emitQmlPropertyNotifications();
    }
}

void AudioRouter::setDestinationDelayMs(const QString& endpointId, double delayMs)
{
    if (endpoints_ == nullptr) {
        return;
    }
    const AudioEndpoint* endpoint = endpoints_->findById(endpointId);
    if (endpoint == nullptr) {
        emit routeError(currentRouteId(), RouteError::DestinationNotFound, QStringLiteral("Unknown endpoint"));
        emitQmlPropertyNotifications();
        return;
    }
    const double ms = std::clamp(std::isfinite(delayMs) ? delayMs : 0.0, 0.0, 250.0);
    if (ms <= 0.0) {
        playgroundPads_.remove(endpointId);
        pendingDelayDestroys_.insert(endpointId);
    } else {
        playgroundPads_.insert(endpointId, ms);
        pendingDelayDestroys_.remove(endpointId);
    }
    const VolumeApplyResult result = volume_.setDestinationDelayMs(endpoint->pipeWireObjectId, endpointId, ms);
    if (!result.allSucceeded) {
        qCWarning(auralisAudio) << "AudioRouter DestinationDelayFailed endpoint=" << endpointId
                                << "node=" << endpoint->pipeWireObjectId << result.error.detail;
        emit routeError(currentRouteId(), result.error.category, result.error.detail);
        emitQmlPropertyNotifications();
    }
    scheduleDelayGraphCommit();
}

void AudioRouter::setRouteVolume(const QString& routeId, double value)
{
    AudioRoute* route = mutableRoute(routeId);
    if (route == nullptr) {
        return;
    }
    if (rejectPlannerMutation(*route)) {
        return;
    }
    route->volume = VolumeController::clamp(value);
    const VolumeApplyResult result = volume_.setRouteVolume(destinationNodes(*route), route->volume);
    emitRouteSignals(*route);
    if (!result.allSucceeded) {
        emit routeError(routeId, result.error.category, result.error.detail);
        emitQmlPropertyNotifications();
    }
}

void AudioRouter::setRouteMuted(const QString& routeId, bool muted)
{
    AudioRoute* route = mutableRoute(routeId);
    if (route == nullptr) {
        return;
    }
    if (rejectPlannerMutation(*route)) {
        return;
    }
    route->muted = muted;
    const VolumeApplyResult result = volume_.setRouteMuted(destinationNodes(*route), muted);
    emitRouteSignals(*route);
    if (!result.allSucceeded) {
        emit routeError(routeId, result.error.category, result.error.detail);
        emitQmlPropertyNotifications();
    }
}

void AudioRouter::refreshSources()
{
    QVector<AudioSource> next = store_ != nullptr ? classifyAudioSources(*store_) : QVector<AudioSource>{};
    if (sources_ == next) {
        return;
    }
    sources_ = std::move(next);
    if (sourceModel_ != nullptr) {
        sourceModel_->setSources(sources_);
    }
    emit sourcesChanged();
}

void AudioRouter::handleGraphChanged()
{
    refreshSources();
    for (AudioRoute& route : routes_) {
        refreshOwnedLinkIds(route);
        if (!route.enabled) {
            continue;
        }
        if (route.state == RouteState::Activating) {
            if (route.ownerType == RouteOwnerType::Session) {
                tryActivateCompensatedFanout(route);
            }
            if (linksHaveError(route)) {
                rollback(route, RouteError::LinkEnteredErrorState, QStringLiteral("A created link entered ERROR"), true);
                continue;
            }
            refreshOwnedLinkIds(route);
            if (!linksOperational(route) && !route.ownedLinks.isEmpty() && route.activatedAt.isValid()
                && route.activatedAt.msecsTo(QDateTime::currentDateTimeUtc()) > 300) {
                bool anyLive = false;
                for (const OwnedLink& owned : route.ownedLinks) {
                    const quint32 globalId =
                        owned.globalId != 0 ? owned.globalId
                                            : (backend_ != nullptr ? backend_->ownedLinkGlobalId(owned.ownershipToken)
                                                                   : 0);
                    if (globalId != 0) {
                        anyLive = true;
                        break;
                    }
                }
                if (!anyLive) {
                    rollback(
                        route,
                        RouteError::LinkCreationFailed,
                        QStringLiteral("Link creation rejected by the audio backend"),
                        true);
                    continue;
                }
            }
            finishActivationIfReady(route);
            continue;
        }
        if (route.state == RouteState::Active || route.state == RouteState::Degraded) {
            const bool sourcePresent = std::any_of(sources_.cbegin(), sources_.cend(), [&](const AudioSource& source) {
                return source.id == route.sourceId;
            });
            bool destPresent = true;
            if (endpoints_ != nullptr) {
                for (const QString& destId : route.destinationIds) {
                    if (endpoints_->findById(destId) == nullptr) {
                        destPresent = false;
                        break;
                    }
                }
            }
            if (!sourcePresent) {
                invalidateOwnedLinks(route);
                setError(route, RouteError::SourceRemoved, QStringLiteral("Source disappeared"));
                setState(route, RouteState::Degraded);
                continue;
            }
            if (!destPresent) {
                invalidateOwnedLinks(route);
                setError(route, RouteError::DestinationRemoved, QStringLiteral("Destination disappeared"));
                setState(route, RouteState::Degraded);
                continue;
            }
            if (linksHaveError(route)) {
                setError(route, RouteError::LinkEnteredErrorState, QStringLiteral("Owned link failed"));
                setState(route, RouteState::Degraded);
                continue;
            }
            if (route.recoveryPolicy == RouteRecoveryPolicy::RebindOnGraphReplacement) {
                if (route.ownerType == RouteOwnerType::Session && tryActivateCompensatedFanout(route)) {
                    continue;
                }
                replanIfEnabled(route);
            }
        }
    }
}

void AudioRouter::handleConnectionState(PipeWireConnectionState state, bool initialSyncComplete)
{
    connectionState_ = state;
    initialSyncComplete_ = initialSyncComplete;
    if (state == PipeWireConnectionState::Error || state == PipeWireConnectionState::Stopped
        || state == PipeWireConnectionState::Stopping) {
        for (AudioRoute& route : routes_) {
            generations_[route.id] = ++nextGeneration_;
            stopActivationTimeout(route.id);
            invalidateOwnedLinks(route);
            if (route.enabled) {
                setError(route, RouteError::PipeWireDisconnected, QStringLiteral("Audio backend disconnected"));
                setState(route, RouteState::Degraded);
            }
        }
        return;
    }
    if (state == PipeWireConnectionState::Connected && initialSyncComplete_) {
        for (AudioRoute& route : routes_) {
            if (route.enabled) {
                replanIfEnabled(route);
            }
        }
    }
}

void AudioRouter::shutdown()
{
    stopAllActivationTimeouts();
    replanBackups_.clear();
    for (AudioRoute& route : routes_) {
        route.enabled = false;
        links_.destroyLinks(route.ownedLinks);
        route.state = RouteState::Inactive;
        route.fanoutRole = RouteFanoutRole::None;
    }
    if (delayCommitTimer_ != nullptr) {
        delayCommitTimer_->stop();
    }
    if (backend_ != nullptr) {
        backend_->destroyLatencyCompensatedFanout();
        backend_->destroyAllDelayBridges();
    }
    playgroundPads_.clear();
    pendingDelayDestroys_.clear();
    routes_.clear();
    sources_.clear();
    if (sourceModel_ != nullptr) {
        sourceModel_->setSources({});
    }
    emitQmlPropertyNotifications();
}

void AudioRouter::setState(AudioRoute& route, RouteState state)
{
    if (route.state == state) {
        emitQmlPropertyNotifications();
        return;
    }
    route.state = state;
    qCInfo(auralisAudio) << "AudioRouter RouteState id=" << route.id << toString(state);
    emit routeStateChanged(route.id, state);
    emitRouteSignals(route);
}

void AudioRouter::setError(AudioRoute& route, RouteError category, const QString& detail)
{
    route.error = {category, detail};
    qCWarning(auralisAudio) << "AudioRouter RouteError id=" << route.id << toString(category) << detail;
    emit routeError(route.id, category, detail);
    emitQmlPropertyNotifications();
}

void AudioRouter::emitRouteSignals(const AudioRoute& route)
{
    emit routeChanged(route.id);
    emitQmlPropertyNotifications();
}

void AudioRouter::emitQmlPropertyNotifications()
{
    const QString id = currentRouteId();
    if (qmlCurrentRouteId_ != id) {
        qmlCurrentRouteId_ = id;
        emit currentRouteIdChanged();
    }
    const QString stateText = routeStateText();
    if (qmlRouteStateText_ != stateText) {
        qmlRouteStateText_ = stateText;
        emit routeStateTextChanged();
    }
    const QString errorText = lastErrorText();
    if (qmlLastErrorText_ != errorText) {
        qmlLastErrorText_ = errorText;
        emit lastErrorTextChanged();
    }
    const bool enabled = routeEnabled();
    if (qmlRouteEnabled_ != enabled) {
        qmlRouteEnabled_ = enabled;
        emit routeEnabledChanged();
    }
    const double volume = routeVolume();
    if (qmlRouteVolume_ != volume) {
        qmlRouteVolume_ = volume;
        emit routeVolumeChanged();
    }
    const bool muted = routeMuted();
    if (qmlRouteMuted_ != muted) {
        qmlRouteMuted_ = muted;
        emit routeMutedChanged();
    }
    const bool capable = volumeCapable();
    if (qmlVolumeCapable_ != capable) {
        qmlVolumeCapable_ = capable;
        emit volumeCapableChanged();
    }
    const int owned = ownedLinkCount();
    if (qmlOwnedLinkCount_ != owned) {
        qmlOwnedLinkCount_ = owned;
        emit ownedLinkCountChanged();
    }
}

bool AudioRouter::validateSelection(const QString& sourceId, const QStringList& destinationIds, RouteErrorInfo* error) const
{
    if (sourceId.isEmpty()) {
        if (error != nullptr) {
            *error = {RouteError::SourceNotFound, QStringLiteral("Source id is empty")};
        }
        return false;
    }
    if (destinationIds.isEmpty()) {
        if (error != nullptr) {
            *error = {RouteError::DestinationNotFound, QStringLiteral("Destination list is empty")};
        }
        return false;
    }
    if (destinationIds.contains(sourceId)) {
        if (error != nullptr) {
            *error = {RouteError::UnsupportedDirection, QStringLiteral("Source selected as destination")};
        }
        return false;
    }

    // Windows process loopback captures a copy; it does not remove the
    // application's original render path. Refuse to play that copy into the
    // current OS default endpoint because doing so creates echo/comb filtering
    // and unavoidable relative latency. A true virtual output endpoint will
    // replace copy mode once the signed driver is installed.
    const AudioSource* selectedSource = nullptr;
    for (const AudioSource& source : sources_) {
        if (source.id == sourceId) {
            selectedSource = &source;
            break;
        }
    }
    const PipeWireNodeInfo* sourceNode = selectedSource != nullptr && store_ != nullptr
        ? store_->node(selectedSource->pipeWireNodeId)
        : nullptr;
    const bool isCopyCapture = sourceNode != nullptr
        && sourceNode->properties.value(QStringLiteral("auralis.capture.mode")) == QLatin1String("copy");
    if (isCopyCapture && endpoints_ != nullptr && store_ != nullptr) {
        for (const QString& destinationId : destinationIds) {
            const AudioEndpoint* endpoint = endpoints_->findById(destinationId);
            const PipeWireNodeInfo* destinationNode = endpoint != nullptr
                ? store_->node(endpoint->pipeWireObjectId)
                : nullptr;
            if (destinationNode != nullptr
                && destinationNode->properties.boolValue(QStringLiteral("device.default")).value_or(false)) {
                if (error != nullptr) {
                    *error = {
                        RouteError::UnsupportedDirection,
                        QStringLiteral(
                            "Duplicate audio prevented: this application already plays to the selected Windows "
                            "default output. Choose a different Windows output, remove this device from the "
                            "Auralis session, or use Auralis Virtual Output when its signed driver is installed.")};
                }
                qCWarning(auralisAudio) << "AudioRouter DuplicatePlaybackPrevented source=" << sourceId
                                        << "destination=" << destinationId;
                return false;
            }
        }
    }
    return true;
}

void AudioRouter::beginActivation(AudioRoute& route, quint64 generation)
{
    route.enabled = true;
    setState(route, RouteState::Planning);
    if (store_ == nullptr || endpoints_ == nullptr) {
        rollback(route, RouteError::InternalError, QStringLiteral("Graph is not available"), true);
        return;
    }
    if (tryActivateCompensatedFanout(route)) {
        return;
    }
    route.fanoutRole = RouteFanoutRole::None;

    const ResolvedRoutePlan planned = planner_.plan(route.sourceId, route.destinationIds, *store_, *endpoints_);
    if (planned.error.hasError()) {
        rollback(route, planned.error.category, planned.error.detail, true);
        return;
    }
    applyDelayBridges(
        route.ownerType == RouteOwnerType::Session ? sessionGroup(route.ownerId) : QVector<AudioRoute*>{&route});
    const ResolvedRoutePlan plan = expandPlanThroughDelayBridges(planned);
    if (QVector<OwnedLink> adopted = tryAdoptExistingLinks(route.id, plan); !adopted.isEmpty()) {
        if (adopted.size() != plan.pairs.size()) {
            rollback(
                route,
                RouteError::PartialActivationFailed,
                QStringLiteral("Only part of the route could be adopted from existing audio links"),
                true);
            return;
        }
        route.ownedLinks = std::move(adopted);
        route.activatedAt = QDateTime::currentDateTimeUtc();
        route.error = {};
        setState(route, RouteState::Active);
        qCInfo(auralisAudio) << "AudioRouter RouteActive id=" << route.id << "links=" << route.ownedLinks.size()
                             << "adopted=true";
        return;
    }
    clearConflictingLinks(plan);
    for (const ResolvedPortPair& pair : plan.pairs) {
        if (store_->findConflictingLinkGlobalId(
                pair.outputNodeId, pair.outputPortId, pair.inputNodeId, pair.inputPortId)
                .has_value()) {
            rollback(
                route,
                RouteError::LinkCreationFailed,
                QStringLiteral("Audio port already linked — stop other playback or deactivate first"),
                true);
            return;
        }
    }
    setState(route, RouteState::Ready);
    setState(route, RouteState::Activating);
    RouteErrorInfo error;
    QVector<OwnedLink> created = links_.createLinks(route.id, plan, &error);
    if (generations_.value(route.id) != generation) {
        links_.destroyLinks(created);
        return;
    }
    if (created.isEmpty()) {
        rollback(
            route,
            error.category == RouteError::None ? RouteError::PartialActivationFailed : error.category,
            error.detail,
            true);
        return;
    }
    route.ownedLinks = std::move(created);
    route.activatedAt = QDateTime::currentDateTimeUtc();
    armActivationTimeout(route.id, generation);
    finishActivationIfReady(route);
}

void AudioRouter::clearConflictingLinks(const ResolvedRoutePlan& plan)
{
    if (store_ == nullptr || backend_ == nullptr) {
        return;
    }
    for (const ResolvedPortPair& pair : plan.pairs) {
        if (store_->findExactLinkGlobalId(
                pair.outputNodeId, pair.outputPortId, pair.inputNodeId, pair.inputPortId)
                .has_value()) {
            continue;
        }
        for (int pass = 0; pass < 8; ++pass) {
            // Only clear foreign producers on the destination input. Output fan-out is allowed.
            std::optional<quint32> conflict = store_->findConflictingLinkGlobalId(
                pair.outputNodeId, pair.outputPortId, pair.inputNodeId, pair.inputPortId);
            if (!conflict.has_value()) {
                conflict = store_->findAnyLinkOnPort(pair.inputNodeId, pair.inputPortId, false);
                if (conflict.has_value()) {
                    const std::optional<quint32> exact = store_->findExactLinkGlobalId(
                        pair.outputNodeId, pair.outputPortId, pair.inputNodeId, pair.inputPortId);
                    if (exact.has_value() && *exact == *conflict) {
                        conflict.reset();
                    }
                }
            }
            if (!conflict.has_value()) {
                break;
            }
            if (isLinkOwnedByAnyRoute(*conflict)) {
                qCInfo(auralisAudio) << "AudioRouter SkippingOwnedPeerLink id=" << *conflict
                                     << "planned out=" << pair.outputNodeId << ":" << pair.outputPortId
                                     << "in=" << pair.inputNodeId << ":" << pair.inputPortId;
                break;
            }
            qCInfo(auralisAudio) << "AudioRouter ClearingConflictingLink id=" << *conflict
                                 << "planned out=" << pair.outputNodeId << ":" << pair.outputPortId
                                 << "in=" << pair.inputNodeId << ":" << pair.inputPortId;
            if (!backend_->destroyForeignLink(*conflict)) {
                qCWarning(auralisAudio) << "AudioRouter ForeignLinkDestroyRefused id=" << *conflict;
                break;
            }
            store_->remove(*conflict);
        }
    }
}

bool AudioRouter::isLinkOwnedByAnyRoute(quint32 globalId) const
{
    if (globalId == 0) {
        return false;
    }
    for (const AudioRoute& route : routes_) {
        for (const OwnedLink& owned : route.ownedLinks) {
            if (owned.globalId == globalId) {
                return true;
            }
            if (backend_ != nullptr && owned.ownershipToken != 0
                && backend_->ownedLinkGlobalId(owned.ownershipToken) == globalId) {
                return true;
            }
        }
    }
    if (store_ != nullptr) {
        if (const PipeWireLinkInfo* link = store_->link(globalId)) {
            if (!link->properties.value(QStringLiteral("auralis.route.id")).isEmpty()) {
                return true;
            }
        }
    }
    return false;
}

QVector<OwnedLink> AudioRouter::tryAdoptExistingLinks(const QString& routeId, const ResolvedRoutePlan& plan)
{
    QVector<OwnedLink> adopted;
    if (store_ == nullptr) {
        return adopted;
    }
    adopted.reserve(plan.pairs.size());
    for (const ResolvedPortPair& pair : plan.pairs) {
        const std::optional<quint32> existing = store_->findExactLinkGlobalId(
            pair.outputNodeId, pair.outputPortId, pair.inputNodeId, pair.inputPortId);
        if (!existing.has_value()) {
            adopted.clear();
            return adopted;
        }
        OwnedLink owned;
        owned.routeId = routeId;
        owned.destinationId = pair.destinationId;
        owned.outputNodeId = pair.outputNodeId;
        owned.outputPortId = pair.outputPortId;
        owned.inputNodeId = pair.inputNodeId;
        owned.inputPortId = pair.inputPortId;
        owned.globalId = *existing;
        owned.channel = pair.channel;
        adopted.push_back(owned);
    }
    return adopted;
}

void AudioRouter::handleOwnedLinkError(quint64 ownershipToken, const QString& detail)
{
    if (ownershipToken == 0) {
        return;
    }
    for (AudioRoute& route : routes_) {
        if (route.state != RouteState::Activating) {
            continue;
        }
        for (const OwnedLink& owned : route.ownedLinks) {
            if (owned.ownershipToken != ownershipToken) {
                continue;
            }
            rollback(route, RouteError::LinkCreationFailed, detail, true);
            emitRouteSignals(route);
            emitQmlPropertyNotifications();
            return;
        }
    }
}

void AudioRouter::finishActivationIfReady(AudioRoute& route)
{
    refreshOwnedLinkIds(route);
    if (!linksOperational(route)) {
        return;
    }
    stopActivationTimeout(route.id);
    if (QVector<OwnedLink> previous = replanBackups_.take(route.id); !previous.isEmpty()) {
        links_.destroyLinks(previous);
    }
    route.error = {};
    if (route.state != RouteState::Active) {
        setState(route, RouteState::Active);
        qCInfo(auralisAudio) << "AudioRouter RouteActive id=" << route.id << "links=" << route.ownedLinks.size();
    }
}

void AudioRouter::rollback(AudioRoute& route, RouteError category, const QString& detail, bool disable)
{
    stopActivationTimeout(route.id);
    links_.destroyLinks(route.ownedLinks);
    route.ownedLinks.clear();
    const RouteOwnerType ownerType = route.ownerType;
    const QString ownerId = route.ownerId;

    if (QVector<OwnedLink> backup = replanBackups_.take(route.id); !backup.isEmpty()) {
        route.ownedLinks = backup;
        refreshOwnedLinkIds(route);
        if (linksOperational(route)) {
            route.enabled = true;
            route.error = {};
            setState(route, RouteState::Active);
            qCInfo(auralisAudio) << "AudioRouter ReplanRestoredPreviousLinks id=" << route.id;
            return;
        }
        links_.destroyLinks(route.ownedLinks);
        route.ownedLinks.clear();
    }

    route.fanoutRole = RouteFanoutRole::None;
    if (disable) {
        route.enabled = false;
        setError(route, category, detail);
        setState(route, RouteState::Failed);
        if (ownerType == RouteOwnerType::Session) {
            releaseFanoutIfUnused(ownerId);
        }
        return;
    }
    setError(route, category, detail);
    setState(route, RouteState::Degraded);
    if (ownerType == RouteOwnerType::Session) {
        releaseFanoutIfUnused(ownerId);
    }
}

void AudioRouter::replanIfEnabled(AudioRoute& route)
{
    if (!route.enabled || store_ == nullptr || endpoints_ == nullptr) {
        return;
    }
    if (connectionState_ != PipeWireConnectionState::Connected || !initialSyncComplete_) {
        return;
    }
    if (tryActivateCompensatedFanout(route)) {
        return;
    }
    if (route.ownerType == RouteOwnerType::Session && backend_ != nullptr) {
        backend_->destroyLatencyCompensatedFanout();
    }
    route.fanoutRole = RouteFanoutRole::None;
    applyDelayBridges(
        route.ownerType == RouteOwnerType::Session ? sessionGroup(route.ownerId) : QVector<AudioRoute*>{&route});
    const ResolvedRoutePlan planned = planner_.plan(route.sourceId, route.destinationIds, *store_, *endpoints_);
    if (planned.error.hasError()) {
        if (linksOperational(route)) {
            return;
        }
        setError(route, planned.error.category, planned.error.detail);
        setState(route, RouteState::Degraded);
        return;
    }
    const ResolvedRoutePlan plan = expandPlanThroughDelayBridges(planned);

    refreshOwnedLinkIds(route);
    bool needsRebuild = route.ownedLinks.size() != plan.pairs.size() || !linksOperational(route);
    if (!needsRebuild) {
        for (int i = 0; i < plan.pairs.size(); ++i) {
            const ResolvedPortPair& pair = plan.pairs.at(i);
            const OwnedLink& owned = route.ownedLinks.at(i);
            if (owned.outputPortId != pair.outputPortId || owned.inputPortId != pair.inputPortId
                || owned.outputNodeId != pair.outputNodeId || owned.inputNodeId != pair.inputNodeId) {
                needsRebuild = true;
                break;
            }
        }
    }
    if (!needsRebuild) {
        if (route.state == RouteState::Degraded && linksOperational(route)) {
            route.error = {};
            setState(route, RouteState::Active);
        }
        return;
    }

    const QVector<OwnedLink> previousLinks = route.ownedLinks;
    const bool previousOperational = linksOperational(route);

    const quint64 generation = ++nextGeneration_;
    generations_[route.id] = generation;
    stopActivationTimeout(route.id);

    route.enabled = true;
    setState(route, RouteState::Planning);
    RouteErrorInfo error;
    QVector<OwnedLink> created = links_.createLinks(route.id, plan, &error);
    if (generations_.value(route.id) != generation) {
        links_.destroyLinks(created);
        return;
    }
    if (created.isEmpty()) {
        if (previousOperational) {
            route.ownedLinks = previousLinks;
            route.error = {};
            setState(route, RouteState::Active);
        } else {
            rollback(route, error.category, error.detail, true);
        }
        return;
    }

    if (previousOperational && !previousLinks.isEmpty()) {
        replanBackups_.insert(route.id, previousLinks);
    }
    route.ownedLinks = std::move(created);
    route.activatedAt = QDateTime::currentDateTimeUtc();
    setState(route, RouteState::Ready);
    setState(route, RouteState::Activating);
    armActivationTimeout(route.id, generation);
    finishActivationIfReady(route);
}

bool AudioRouter::linksOperational(const AudioRoute& route) const
{
    if (route.fanoutRole == RouteFanoutRole::Member) {
        return fanoutSinkReady();
    }
    if (route.ownedLinks.isEmpty() || store_ == nullptr) {
        return false;
    }
    for (const OwnedLink& owned : route.ownedLinks) {
        quint32 globalId = owned.globalId;
        if (globalId == 0 && owned.ownershipToken != 0 && backend_ != nullptr) {
            globalId = backend_->ownedLinkGlobalId(owned.ownershipToken);
        }
        if (globalId == 0) {
            return false;
        }
        const PipeWireLinkInfo* info = store_->link(globalId);
        if (info == nullptr) {
            return false;
        }
        if (info->state != PipeWireLinkState::Active && info->state != PipeWireLinkState::Paused) {
            return false;
        }
    }
    return true;
}

bool AudioRouter::linksHaveError(const AudioRoute& route) const
{
    if (store_ == nullptr) {
        return false;
    }
    for (const OwnedLink& owned : route.ownedLinks) {
        if (owned.ownershipToken == 0) {
            continue;
        }
        const quint32 globalId =
            owned.globalId != 0 ? owned.globalId
                                : (backend_ != nullptr ? backend_->ownedLinkGlobalId(owned.ownershipToken) : 0);
        if (globalId == 0) {
            continue;
        }
        const PipeWireLinkInfo* info = store_->link(globalId);
        if (info != nullptr && info->state == PipeWireLinkState::Error) {
            return true;
        }
    }
    return false;
}

void AudioRouter::refreshOwnedLinkIds(AudioRoute& route)
{
    links_.refreshGlobalIds(route.ownedLinks);
}

void AudioRouter::armActivationTimeout(const QString& routeId, quint64 generation)
{
    stopActivationTimeout(routeId);
    auto* timer = new QTimer(this);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, this, [this, routeId, generation]() {
        if (generations_.value(routeId) != generation) {
            return;
        }
        AudioRoute* route = mutableRoute(routeId);
        if (route == nullptr || route->state != RouteState::Activating) {
            return;
        }
        rollback(*route, RouteError::LinkCreationFailed, QStringLiteral("Activation timed out"), true);
    });
    activationTimers_.insert(routeId, timer);
    timer->start(activationTimeoutMs_);
}

void AudioRouter::stopActivationTimeout(const QString& routeId)
{
    if (QTimer* timer = activationTimers_.take(routeId)) {
        timer->stop();
        timer->deleteLater();
    }
}

void AudioRouter::stopAllActivationTimeouts()
{
    const QList<QString> ids = activationTimers_.keys();
    for (const QString& routeId : ids) {
        stopActivationTimeout(routeId);
    }
}

void AudioRouter::invalidateOwnedLinks(AudioRoute& route)
{
    if (!hasLiveOwnership(route) && route.ownedLinks.isEmpty()) {
        return;
    }
    links_.destroyLinks(route.ownedLinks);
}

QVector<QPair<QString, quint32>> AudioRouter::destinationNodes(const AudioRoute& route) const
{
    QVector<QPair<QString, quint32>> result;
    if (endpoints_ == nullptr) {
        return result;
    }
    for (const QString& destId : route.destinationIds) {
        if (const AudioEndpoint* endpoint = endpoints_->findById(destId)) {
            result.push_back({destId, endpoint->pipeWireObjectId});
        }
    }
    return result;
}

QVector<AudioRoute*> AudioRouter::sessionGroup(const QString& ownerId)
{
    QVector<AudioRoute*> group;
    if (ownerId.isEmpty()) {
        return group;
    }
    for (AudioRoute& route : routes_) {
        if (route.ownerType != RouteOwnerType::Session || route.ownerId != ownerId) {
            continue;
        }
        if (route.enabled || route.state == RouteState::Planning || route.state == RouteState::Ready
            || route.state == RouteState::Activating) {
            group.push_back(&route);
        }
    }
    std::sort(group.begin(), group.end(), [](const AudioRoute* left, const AudioRoute* right) {
        return left->id < right->id;
    });
    return group;
}

QStringList AudioRouter::sessionSinkNodeNames(const QVector<AudioRoute*>& group) const
{
    QStringList names;
    if (endpoints_ == nullptr) {
        return names;
    }
    for (const AudioRoute* route : group) {
        if (route == nullptr) {
            continue;
        }
        for (const QString& destId : route->destinationIds) {
            const AudioEndpoint* endpoint = endpoints_->findById(destId);
            if (endpoint == nullptr) {
                continue;
            }
            QString sinkName = endpoint->nodeName;
            if (sinkName.isEmpty()) {
                sinkName = endpoint->name;
            }
            if (sinkName.isEmpty() && store_ != nullptr) {
                if (const PipeWireNodeInfo* node = store_->node(endpoint->pipeWireObjectId)) {
                    sinkName = node->name;
                }
            }
            if (playgroundPads_.value(destId, 0.0) > 0.0) {
                sinkName = auralisDelayBridgeCaptureNodeName(destId);
            } else if (store_ != nullptr) {
                const DelayBridgeNodes bridge = findDelayBridge(*store_, destId);
                if (bridge.ready) {
                    if (const PipeWireNodeInfo* capture = store_->node(bridge.captureNodeId)) {
                        if (!capture->name.isEmpty()) {
                            sinkName = capture->name;
                        }
                    }
                }
            }
            if (sinkName.isEmpty() || names.contains(sinkName)) {
                continue;
            }
            names.push_back(sinkName);
        }
    }
    return names;
}

quint32 AudioRouter::fanoutSinkNodeId() const
{
    if (store_ == nullptr) {
        return 0;
    }
    for (const PipeWireNodeInfo& node : store_->nodes()) {
        if (isAuralisSessionFanoutSink(node)) {
            return node.globalId;
        }
    }
    return 0;
}

bool AudioRouter::fanoutSinkReady() const
{
    const quint32 id = fanoutSinkNodeId();
    if (id == 0 || store_ == nullptr) {
        return false;
    }
    return findNodeAudioPort(*store_, id, PipeWirePortDirection::Input, {}) != nullptr;
}

bool AudioRouter::tryActivateCompensatedFanout(AudioRoute& route)
{
    if (backend_ == nullptr || route.ownerType != RouteOwnerType::Session || route.ownerId.isEmpty()) {
        return false;
    }
    const QVector<AudioRoute*> group = sessionGroup(route.ownerId);
    if (group.size() < 2) {
        if (route.fanoutRole != RouteFanoutRole::None && backend_ != nullptr) {
            backend_->destroyLatencyCompensatedFanout();
            route.fanoutRole = RouteFanoutRole::None;
        }
        return false;
    }
    applyDelayBridges(group);
    const QStringList names = sessionSinkNodeNames(group);
    if (names.size() < 2 || !backend_->ensureLatencyCompensatedFanout(names)) {
        if (route.fanoutRole != RouteFanoutRole::None) {
            backend_->destroyLatencyCompensatedFanout();
            route.fanoutRole = RouteFanoutRole::None;
        }
        return false;
    }
    relinkSessionToFanout(route.ownerId);
    return true;
}

void AudioRouter::relinkSessionToFanout(const QString& ownerId)
{
    const QVector<AudioRoute*> group = sessionGroup(ownerId);
    if (group.size() < 2 || backend_ == nullptr || store_ == nullptr) {
        return;
    }
    AudioRoute* leader = group.front();
    if (!fanoutSinkReady()) {
        for (AudioRoute* route : group) {
            if (route->fanoutRole == RouteFanoutRole::None && !route->ownedLinks.isEmpty()) {
                links_.destroyLinks(route->ownedLinks);
                route->ownedLinks.clear();
            }
            route->enabled = true;
            route->fanoutRole = (route == leader) ? RouteFanoutRole::Leader : RouteFanoutRole::Member;
            if (route->state != RouteState::Activating && route->state != RouteState::Planning) {
                setState(*route, RouteState::Activating);
            } else if (route->state == RouteState::Planning) {
                setState(*route, RouteState::Activating);
            }
        }
        qCInfo(auralisAudio) << "AudioRouter SessionFanoutWaiting owner=" << ownerId;
        return;
    }

    const quint32 fanout = fanoutSinkNodeId();
    const ResolvedRoutePlan plan =
        planner_.planToSinkNode(leader->sourceId, fanout, QStringLiteral("fanout"), *store_);
    if (plan.error.hasError()) {
        qCWarning(auralisAudio) << "AudioRouter SessionFanoutPlanFailed" << plan.error.detail;
        return;
    }

    refreshOwnedLinkIds(*leader);
    bool leaderHasFanoutLinks = leader->fanoutRole == RouteFanoutRole::Leader && ownedLinksMatchPlan(*leader, plan);
    if (leaderHasFanoutLinks) {
        for (const OwnedLink& owned : leader->ownedLinks) {
            if (owned.inputNodeId != fanout) {
                leaderHasFanoutLinks = false;
                break;
            }
        }
    }

    if (!leaderHasFanoutLinks) {
        for (AudioRoute* route : group) {
            links_.destroyLinks(route->ownedLinks);
            route->ownedLinks.clear();
        }
        clearConflictingLinks(plan);
        RouteErrorInfo error;
        QVector<OwnedLink> created = links_.createLinks(leader->id, plan, &error);
        if (created.isEmpty()) {
            qCWarning(auralisAudio) << "AudioRouter SessionFanoutLinkFailed" << error.detail;
            return;
        }
        leader->ownedLinks = std::move(created);
        leader->activatedAt = QDateTime::currentDateTimeUtc();
        const quint64 generation = ++nextGeneration_;
        generations_[leader->id] = generation;
        leader->fanoutRole = RouteFanoutRole::Leader;
        leader->enabled = true;
        setState(*leader, RouteState::Activating);
        armActivationTimeout(leader->id, generation);
        qCInfo(auralisAudio) << "AudioRouter SessionFanoutLinked owner=" << ownerId << "leader=" << leader->id;
    }
    finishActivationIfReady(*leader);

    for (AudioRoute* route : group) {
        if (route == leader) {
            continue;
        }
        if (!route->ownedLinks.isEmpty()) {
            links_.destroyLinks(route->ownedLinks);
            route->ownedLinks.clear();
        }
        const bool alreadyMember = route->fanoutRole == RouteFanoutRole::Member && route->state == RouteState::Active;
        route->fanoutRole = RouteFanoutRole::Member;
        route->enabled = true;
        route->error = {};
        route->activatedAt = QDateTime::currentDateTimeUtc();
        stopActivationTimeout(route->id);
        if (!alreadyMember) {
            setState(*route, RouteState::Active);
            qCInfo(auralisAudio) << "AudioRouter RouteActive id=" << route->id << "fanout=member";
        }
    }
}

void AudioRouter::releaseFanoutIfUnused(const QString& ownerId)
{
    if (backend_ == nullptr || ownerId.isEmpty()) {
        return;
    }
    const QVector<AudioRoute*> group = sessionGroup(ownerId);
    if (group.size() >= 2) {
        relinkSessionToFanout(ownerId);
        return;
    }
    backend_->destroyLatencyCompensatedFanout();
    backend_->destroyAllDelayBridges();
    for (AudioRoute* route : group) {
        route->fanoutRole = RouteFanoutRole::None;
        if (route->enabled) {
            replanIfEnabled(*route);
        }
    }
}

void AudioRouter::applyDelayBridges(const QVector<AudioRoute*>& group)
{
    if (backend_ == nullptr || endpoints_ == nullptr) {
        return;
    }
    if (delayCommitTimer_ != nullptr && delayCommitTimer_->isActive()) {
        return;
    }
    QHash<QString, QString> destNodeNames;
    for (const AudioRoute* route : group) {
        if (route == nullptr) {
            continue;
        }
        for (const QString& destId : route->destinationIds) {
            if (destNodeNames.contains(destId)) {
                continue;
            }
            const AudioEndpoint* endpoint = endpoints_->findById(destId);
            if (endpoint == nullptr) {
                continue;
            }
            destNodeNames.insert(destId, endpoint->nodeName);
        }
    }
    for (auto it = destNodeNames.constBegin(); it != destNodeNames.constEnd(); ++it) {
        const double ms = playgroundPads_.value(it.key(), 0.0);
        if (ms <= 0.0) {
            backend_->destroyDelayBridge(it.key());
        } else {
            backend_->ensureDelayBridge(it.key(), it.value(), ms / 1000.0);
        }
    }
}

void AudioRouter::scheduleDelayGraphCommit()
{
    if (delayGraphCommitMs_ <= 0) {
        commitDelayGraph();
        return;
    }
    if (delayCommitTimer_ == nullptr) {
        delayCommitTimer_ = new QTimer(this);
        delayCommitTimer_->setSingleShot(true);
        connect(delayCommitTimer_, &QTimer::timeout, this, &AudioRouter::commitDelayGraph);
    }
    delayCommitTimer_->start(delayGraphCommitMs_);
}

void AudioRouter::commitDelayGraph()
{
    if (backend_ != nullptr && endpoints_ != nullptr
        && (delayCommitTimer_ == nullptr || !delayCommitTimer_->isActive())) {
        const QStringList destroyIds = pendingDelayDestroys_.values();
        pendingDelayDestroys_.clear();
        for (const QString& endpointId : destroyIds) {
            if (!playgroundPads_.contains(endpointId)) {
                backend_->destroyDelayBridge(endpointId);
            }
        }
        for (auto it = playgroundPads_.constBegin(); it != playgroundPads_.constEnd(); ++it) {
            const AudioEndpoint* endpoint = endpoints_->findById(it.key());
            if (endpoint == nullptr) {
                continue;
            }
            backend_->ensureDelayBridge(it.key(), endpoint->nodeName, it.value() / 1000.0);
        }
    }
    QStringList sessionIds;
    QStringList plannerRouteIds;
    for (const AudioRoute& route : routes_) {
        if (!route.enabled) {
            continue;
        }
        if (route.ownerType == RouteOwnerType::Session && !route.ownerId.isEmpty()) {
            if (!sessionIds.contains(route.ownerId)) {
                sessionIds.push_back(route.ownerId);
            }
            continue;
        }
        plannerRouteIds.push_back(route.id);
    }
    for (const QString& sessionId : sessionIds) {
        const QVector<AudioRoute*> group = sessionGroup(sessionId);
        applyDelayBridges(group);
        if (group.isEmpty()) {
            continue;
        }
        if (!tryActivateCompensatedFanout(*group.front())) {
            replanIfEnabled(*group.front());
        }
    }
    for (const QString& routeId : plannerRouteIds) {
        AudioRoute* route = mutableRoute(routeId);
        if (route == nullptr) {
            continue;
        }
        applyDelayBridges({route});
        replanIfEnabled(*route);
    }
}

ResolvedRoutePlan AudioRouter::expandPlanThroughDelayBridges(const ResolvedRoutePlan& plan) const
{
    if (store_ == nullptr) {
        return plan;
    }
    ResolvedRoutePlan expanded = plan;
    expanded.pairs.clear();
    for (const ResolvedPortPair& pair : plan.pairs) {
        const DelayBridgeNodes bridge = findDelayBridge(*store_, pair.destinationId);
        if (!bridge.ready) {
            expanded.pairs.push_back(pair);
            continue;
        }
        const PipeWirePortInfo* captureIn =
            findNodeAudioPort(*store_, bridge.captureNodeId, PipeWirePortDirection::Input, pair.channel);
        const PipeWirePortInfo* playbackOut =
            findNodeAudioPort(*store_, bridge.playbackNodeId, PipeWirePortDirection::Output, pair.channel);
        if (captureIn == nullptr || playbackOut == nullptr) {
            expanded.pairs.push_back(pair);
            continue;
        }
        ResolvedPortPair toBridge = pair;
        toBridge.inputNodeId = captureIn->nodeId.value_or(bridge.captureNodeId);
        toBridge.inputPortId = captureIn->globalId;
        expanded.pairs.push_back(toBridge);

        ResolvedPortPair toDest = pair;
        toDest.outputNodeId = playbackOut->nodeId.value_or(bridge.playbackNodeId);
        toDest.outputPortId = playbackOut->globalId;
        expanded.pairs.push_back(toDest);
    }
    return expanded;
}

bool AudioRouter::ownedLinksMatchPlan(const AudioRoute& route, const ResolvedRoutePlan& plan) const
{
    if (route.ownedLinks.size() != plan.pairs.size()) {
        return false;
    }
    for (int i = 0; i < plan.pairs.size(); ++i) {
        const ResolvedPortPair& pair = plan.pairs.at(i);
        const OwnedLink& owned = route.ownedLinks.at(i);
        if (owned.outputPortId != pair.outputPortId || owned.inputPortId != pair.inputPortId
            || owned.outputNodeId != pair.outputNodeId || owned.inputNodeId != pair.inputNodeId) {
            return false;
        }
    }
    return true;
}

} // namespace auralis::audio

#include <auralis/audio/AudioRouter.h>

#include <auralis/audio/AudioSourceListModel.h>
#include <auralis/core/LoggingCategories.h>

#include <QUuid>

#include <algorithm>

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
}

AudioRouter::~AudioRouter()
{
    stopAllActivationTimeouts();
}

QAbstractItemModel* AudioRouter::sources() const
{
    return sourceModel_;
}

int AudioRouter::sourceCount() const
{
    return static_cast<int>(sources_.size());
}

QString AudioRouter::currentRouteId() const
{
    return routes_.isEmpty() ? QString() : routes_.front().id;
}

QString AudioRouter::routeStateText() const
{
    if (routes_.isEmpty()) {
        return toString(RouteState::Inactive);
    }
    return toString(routes_.front().state);
}

QString AudioRouter::lastErrorText() const
{
    if (routes_.isEmpty() || !routes_.front().error.hasError()) {
        return {};
    }
    return QStringLiteral("%1: %2").arg(toString(routes_.front().error.category), routes_.front().error.detail);
}

bool AudioRouter::routeEnabled() const
{
    return !routes_.isEmpty() && routes_.front().enabled;
}

double AudioRouter::routeVolume() const
{
    return routes_.isEmpty() ? 1.0 : routes_.front().volume;
}

bool AudioRouter::routeMuted() const
{
    return !routes_.isEmpty() && routes_.front().muted;
}

bool AudioRouter::volumeCapable() const
{
    if (routes_.isEmpty() || endpoints_ == nullptr) {
        return false;
    }
    for (const QString& destId : routes_.front().destinationIds) {
        const AudioEndpoint* endpoint = endpoints_->findById(destId);
        if (endpoint == nullptr || !volume_.volumeSupported(endpoint->pipeWireObjectId)) {
            return false;
        }
    }
    return !routes_.front().destinationIds.isEmpty();
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
    routes_.push_back(route);
    qCInfo(auralisAudio) << "AudioRouter RouteCreated id=" << route.id << "source=" << sourceId
                         << "destinations=" << dests.size();
    emit routeAdded(route.id);
    emitRouteSignals(route);
    return route.id;
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
    if (connectionState_ != PipeWireConnectionState::Connected || !initialSyncComplete_) {
        setError(*route, RouteError::PipeWireDisconnected, QStringLiteral("PipeWire is not connected"));
        setState(*route, RouteState::Failed);
        return;
    }
    const quint64 generation = ++nextGeneration_;
    generations_[routeId] = generation;
    beginActivation(*route, generation);
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
    if (route->state == RouteState::Inactive && route->ownedLinks.isEmpty()) {
        emitQmlPropertyNotifications();
        return;
    }
    setState(*route, RouteState::Deactivating);
    links_.destroyLinks(route->ownedLinks);
    setState(*route, RouteState::Inactive);
    qCInfo(auralisAudio) << "AudioRouter RouteDeactivated id=" << routeId;
}

void AudioRouter::setRouteSource(const QString& routeId, const QString& sourceId)
{
    AudioRoute* route = mutableRoute(routeId);
    if (route == nullptr) {
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

void AudioRouter::setRouteVolume(const QString& routeId, double value)
{
    AudioRoute* route = mutableRoute(routeId);
    if (route == nullptr) {
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
    sources_ = store_ != nullptr ? classifyAudioSources(*store_) : QVector<AudioSource>{};
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
            if (linksHaveError(route)) {
                rollback(route, RouteError::LinkEnteredErrorState, QStringLiteral("A created link entered ERROR"), true);
                continue;
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
                setError(route, RouteError::PipeWireDisconnected, QStringLiteral("PipeWire disconnected"));
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
    for (AudioRoute& route : routes_) {
        route.enabled = false;
        links_.destroyLinks(route.ownedLinks);
        route.state = RouteState::Inactive;
    }
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

    const ResolvedRoutePlan plan = planner_.plan(route.sourceId, route.destinationIds, *store_, *endpoints_);
    if (plan.error.hasError()) {
        rollback(route, plan.error.category, plan.error.detail, true);
        return;
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

void AudioRouter::finishActivationIfReady(AudioRoute& route)
{
    refreshOwnedLinkIds(route);
    if (!linksOperational(route)) {
        return;
    }
    stopActivationTimeout(route.id);
    route.error = {};
    setState(route, RouteState::Active);
    qCInfo(auralisAudio) << "AudioRouter RouteActive id=" << route.id << "links=" << route.ownedLinks.size();
}

void AudioRouter::rollback(AudioRoute& route, RouteError category, const QString& detail, bool disable)
{
    stopActivationTimeout(route.id);
    links_.destroyLinks(route.ownedLinks);
    if (disable) {
        route.enabled = false;
        setError(route, category, detail);
        setState(route, RouteState::Failed);
        return;
    }
    setError(route, category, detail);
    setState(route, RouteState::Degraded);
}

void AudioRouter::replanIfEnabled(AudioRoute& route)
{
    if (!route.enabled || store_ == nullptr || endpoints_ == nullptr) {
        return;
    }
    if (connectionState_ != PipeWireConnectionState::Connected || !initialSyncComplete_) {
        return;
    }
    const ResolvedRoutePlan plan = planner_.plan(route.sourceId, route.destinationIds, *store_, *endpoints_);
    if (plan.error.hasError()) {
        setError(route, plan.error.category, plan.error.detail);
        setState(route, RouteState::Degraded);
        return;
    }

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

    const quint64 generation = ++nextGeneration_;
    generations_[route.id] = generation;
    stopActivationTimeout(route.id);
    links_.destroyLinks(route.ownedLinks);
    beginActivation(route, generation);
}

bool AudioRouter::linksOperational(const AudioRoute& route) const
{
    if (route.ownedLinks.isEmpty() || store_ == nullptr) {
        return false;
    }
    for (const OwnedLink& owned : route.ownedLinks) {
        if (owned.ownershipToken == 0) {
            return false;
        }
        const quint32 globalId =
            owned.globalId != 0 ? owned.globalId
                                : (backend_ != nullptr ? backend_->ownedLinkGlobalId(owned.ownershipToken) : 0);
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

} // namespace auralis::audio

#include <auralis/session/RoutingCoordinator.h>

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/AudioSource.h>
#include <auralis/bluetooth/BlueZTypes.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/LoggingCategories.h>

namespace auralis::session {
namespace {

bool routeActivationPending(auralis::audio::RouteState state)
{
    return state == auralis::audio::RouteState::Planning || state == auralis::audio::RouteState::Ready
        || state == auralis::audio::RouteState::Activating;
}

} // namespace

RoutingCoordinator::RoutingCoordinator(
    auralis::audio::AudioRouter* router,
    auralis::audio::AudioEndpointRegistry* endpoints,
    auralis::bluetooth::DeviceRegistry* devices)
    : router_(router)
    , endpoints_(endpoints)
    , devices_(devices)
{
}

QVector<MemberRouteDesired> RoutingCoordinator::desiredRoutes(const AuralisSession& session) const
{
    QVector<MemberRouteDesired> desired;
    if (session.sourceId.isEmpty()) {
        return desired;
    }
    for (const SessionDevice& device : session.devices) {
        if (!device.enabled) {
            continue;
        }
        const QString endpointId = resolveEndpointId(device.deviceId);
        if (endpointId.isEmpty()) {
            continue;
        }
        desired.push_back({device.deviceId, endpointId});
    }
    return desired;
}

void RoutingCoordinator::reconcile(
    AuralisSession& session,
    quint64 generation,
    const QHash<QString, quint64>& generations,
    ReconcileMode mode)
{
    if (router_ == nullptr || session.sourceId.isEmpty()) {
        return;
    }
    if (generations.value(session.id) != generation) {
        return;
    }
    if (mode == ReconcileMode::SuppressCreate) {
        for (SessionDevice& device : session.devices) {
            if (device.runtime.routeId.isEmpty() || router_ == nullptr) {
                continue;
            }
            router_->deactivateRoute(device.runtime.routeId);
            router_->removeRoute(device.runtime.routeId);
            sessionRouteOwners_.remove(device.runtime.routeId);
            device.runtime.routeId.clear();
            device.runtime.routeActive = false;
        }
        refreshRuntime(session);
        return;
    }

    const QVector<MemberRouteDesired> desired = desiredRoutes(session);
    QHash<QString, MemberRouteDesired> desiredByDevice;
    for (const MemberRouteDesired& item : desired) {
        desiredByDevice.insert(item.deviceId, item);
    }

    for (SessionDevice& device : session.devices) {
        device.runtime.endpointAvailable = false;
        device.runtime.routeActive = false;
        if (!device.enabled) {
            if (!device.runtime.routeId.isEmpty()) {
                router_->deactivateRoute(device.runtime.routeId);
                router_->removeRoute(device.runtime.routeId);
                sessionRouteOwners_.remove(device.runtime.routeId);
                device.runtime.routeId.clear();
            }
            continue;
        }

        if (!desiredByDevice.contains(device.deviceId)) {
            device.runtime.endpointId.clear();
            device.runtime.endpointAvailable = false;
            if (!device.runtime.routeId.isEmpty()) {
                router_->deactivateRoute(device.runtime.routeId);
                router_->removeRoute(device.runtime.routeId);
                sessionRouteOwners_.remove(device.runtime.routeId);
                device.runtime.routeId.clear();
            }
            continue;
        }
        const MemberRouteDesired wanted = desiredByDevice.value(device.deviceId);
        device.runtime.endpointId = wanted.endpointId;
        device.runtime.endpointAvailable = true;

        bool routeMatches = false;
        if (!device.runtime.routeId.isEmpty()) {
            if (const std::optional<auralis::audio::AudioRoute> route = router_->routeById(device.runtime.routeId)) {
                routeMatches = route->sourceId == session.sourceId
                    && route->destinationIds.size() == 1 && route->destinationIds.front() == wanted.endpointId;
                device.runtime.routeActive = route->state == auralis::audio::RouteState::Active;
            } else {
                device.runtime.routeId.clear();
            }
        }

        if (!routeMatches) {
            if (!device.runtime.autoRestoreAllowed && session.state != SessionState::Starting) {
                if (!device.runtime.routeId.isEmpty()) {
                    router_->deactivateRoute(device.runtime.routeId);
                    router_->removeRoute(device.runtime.routeId);
                    sessionRouteOwners_.remove(device.runtime.routeId);
                    device.runtime.routeId.clear();
                }
                continue;
            }
            if (!device.runtime.routeId.isEmpty()) {
                router_->deactivateRoute(device.runtime.routeId);
                router_->removeRoute(device.runtime.routeId);
                sessionRouteOwners_.remove(device.runtime.routeId);
                device.runtime.routeId.clear();
            }
            const QString routeId = router_->createSessionRoute(session.id, session.sourceId, {wanted.endpointId});
            if (routeId.isEmpty()) {
                device.runtime.routeRequested = false;
                device.runtime.routeActive = false;
                device.runtime.lastError = {SessionError::RouteCreationFailed, QStringLiteral("Failed to create route")};
                continue;
            }
            device.runtime.routeId = routeId;
            sessionRouteOwners_.insert(routeId, session.id);
            device.runtime.routeRequested = true;
            device.runtime.lastError = {};
            router_->activateRoute(routeId);
        } else if (!device.runtime.routeActive) {
            if (!device.runtime.autoRestoreAllowed && session.state != SessionState::Starting) {
                qCInfo(auralisSession) << "RouteRecoverySuppressedByPolicy" << session.id << device.deviceId;
                continue;
            }
            if (const std::optional<auralis::audio::AudioRoute> route = router_->routeById(device.runtime.routeId)) {
                if (!routeActivationPending(route->state)) {
                    router_->activateRoute(device.runtime.routeId);
                }
            }
        }
    }

    refreshRuntime(session);
}

void RoutingCoordinator::stopSessionRoutes(AuralisSession& session)
{
    if (router_ == nullptr) {
        return;
    }
    for (SessionDevice& device : session.devices) {
        if (device.runtime.routeId.isEmpty()) {
            continue;
        }
        router_->deactivateRoute(device.runtime.routeId);
        router_->removeRoute(device.runtime.routeId);
        sessionRouteOwners_.remove(device.runtime.routeId);
        device.runtime.routeId.clear();
        device.runtime.routeRequested = false;
        device.runtime.routeActive = false;
    }
}

bool RoutingCoordinator::sourceAvailable(const AuralisSession& session) const
{
    if (router_ == nullptr || session.sourceId.isEmpty()) {
        return false;
    }
    for (const auralis::audio::AudioSource& source : router_->sourceList()) {
        if (source.id == session.sourceId && source.available) {
            return true;
        }
    }
    return false;
}

bool RoutingCoordinator::memberEndpointAvailable(const QString& deviceId) const
{
    return !resolveEndpointId(deviceId).isEmpty();
}

bool RoutingCoordinator::memberConnected(const QString& deviceId) const
{
    if (devices_ == nullptr) {
        return false;
    }
    const QString normalized = deviceId.trimmed().toUpper();
    for (const auralis::bluetooth::BluetoothDeviceData& device : devices_->devices()) {
        if (device.address.trimmed().toUpper() == normalized) {
            return device.connected;
        }
    }
    return false;
}

void RoutingCoordinator::refreshRuntime(AuralisSession& session)
{
    for (SessionDevice& device : session.devices) {
        device.runtime.connected = memberConnected(device.deviceId);
        device.runtime.endpointAvailable = memberEndpointAvailable(device.deviceId);
        device.runtime.routeActive = false;
        if (device.runtime.routeId.isEmpty() || router_ == nullptr) {
            continue;
        }
        if (const std::optional<auralis::audio::AudioRoute> route = router_->routeById(device.runtime.routeId)) {
            device.runtime.routeActive = route->state == auralis::audio::RouteState::Active;
            if (!device.runtime.endpointId.isEmpty()) {
                device.runtime.endpointId = route->destinationIds.value(0);
            }
        }
    }
}

QString RoutingCoordinator::resolveEndpointId(const QString& deviceId) const
{
    if (endpoints_ == nullptr) {
        return {};
    }
    const QString normalized = deviceId.trimmed().toUpper();
    for (const auralis::audio::AudioEndpoint& endpoint : endpoints_->playbackEndpoints()) {
        if (endpoint.availability != auralis::audio::AudioEndpointAvailability::Available) {
            continue;
        }
        if (endpoint.bluetoothAddress.trimmed().toUpper() == normalized) {
            return endpoint.id;
        }
    }
    return {};
}

} // namespace auralis::session

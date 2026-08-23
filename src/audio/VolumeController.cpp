#include <auralis/audio/VolumeController.h>

#include <algorithm>
#include <cmath>

namespace auralis::audio {

VolumeController::VolumeController(IPipeWireLinkBackend* backend)
    : backend_(backend)
{
}

double VolumeController::clamp(double value)
{
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, 0.0, 1.0);
}

VolumeApplyResult VolumeController::setDestinationVolume(quint32 nodeId, const QString& endpointId, double value)
{
    VolumeApplyResult result;
    if (backend_ == nullptr) {
        result.allSucceeded = false;
        result.error = {RouteError::InternalError, QStringLiteral("Volume backend is missing")};
        result.failedEndpointIds.push_back(endpointId);
        return result;
    }
    if (!backend_->volumeSupported(nodeId)) {
        result.allSucceeded = false;
        result.error = {RouteError::VolumeControlUnsupported, QStringLiteral("Volume is not supported")};
        result.failedEndpointIds.push_back(endpointId);
        return result;
    }
    if (!backend_->setNodeVolume(nodeId, clamp(value))) {
        result.allSucceeded = false;
        result.error = {RouteError::VolumeControlFailed, QStringLiteral("Failed to set volume")};
        result.failedEndpointIds.push_back(endpointId);
    }
    return result;
}

VolumeApplyResult VolumeController::setDestinationDelayMs(quint32 nodeId, const QString& endpointId, double delayMs)
{
    VolumeApplyResult result;
    if (backend_ == nullptr) {
        result.allSucceeded = false;
        result.error = {RouteError::InternalError, QStringLiteral("Delay backend is missing")};
        result.failedEndpointIds.push_back(endpointId);
        return result;
    }
    double ms = delayMs;
    if (!std::isfinite(ms)) {
        ms = 0.0;
    }
    ms = std::clamp(ms, 0.0, 250.0);
    if (!backend_->setNodeDelaySeconds(nodeId, ms / 1000.0)) {
        result.allSucceeded = false;
        result.error = {RouteError::InternalError, QStringLiteral("Failed to set destination delay")};
        result.failedEndpointIds.push_back(endpointId);
    }
    return result;
}

VolumeApplyResult VolumeController::setDestinationMuted(quint32 nodeId, const QString& endpointId, bool muted)
{
    VolumeApplyResult result;
    if (backend_ == nullptr) {
        result.allSucceeded = false;
        result.error = {RouteError::InternalError, QStringLiteral("Volume backend is missing")};
        result.failedEndpointIds.push_back(endpointId);
        return result;
    }
    if (!backend_->volumeSupported(nodeId)) {
        result.allSucceeded = false;
        result.error = {RouteError::VolumeControlUnsupported, QStringLiteral("Mute is not supported")};
        result.failedEndpointIds.push_back(endpointId);
        return result;
    }
    if (!backend_->setNodeMuted(nodeId, muted)) {
        result.allSucceeded = false;
        result.error = {RouteError::VolumeControlFailed, QStringLiteral("Failed to set mute")};
        result.failedEndpointIds.push_back(endpointId);
    }
    return result;
}

VolumeApplyResult VolumeController::setRouteVolume(const QVector<QPair<QString, quint32>>& destinations, double value)
{
    VolumeApplyResult result;
    const double clamped = clamp(value);
    for (const auto& dest : destinations) {
        const VolumeApplyResult one = setDestinationVolume(dest.second, dest.first, clamped);
        if (!one.allSucceeded) {
            result.allSucceeded = false;
            result.failedEndpointIds.append(one.failedEndpointIds);
            result.error = one.error;
        }
    }
    if (!result.allSucceeded && result.failedEndpointIds.size() < destinations.size()) {
        result.error.category = RouteError::VolumeControlFailed;
        result.error.detail = QStringLiteral("Volume applied to some destinations only");
    }
    return result;
}

VolumeApplyResult VolumeController::setRouteMuted(const QVector<QPair<QString, quint32>>& destinations, bool muted)
{
    VolumeApplyResult result;
    for (const auto& dest : destinations) {
        const VolumeApplyResult one = setDestinationMuted(dest.second, dest.first, muted);
        if (!one.allSucceeded) {
            result.allSucceeded = false;
            result.failedEndpointIds.append(one.failedEndpointIds);
            result.error = one.error;
        }
    }
    if (!result.allSucceeded && result.failedEndpointIds.size() < destinations.size()) {
        result.error.category = RouteError::VolumeControlFailed;
        result.error.detail = QStringLiteral("Mute applied to some destinations only");
    }
    return result;
}

bool VolumeController::volumeSupported(quint32 nodeId) const
{
    return backend_ != nullptr && backend_->volumeSupported(nodeId);
}

} // namespace auralis::audio

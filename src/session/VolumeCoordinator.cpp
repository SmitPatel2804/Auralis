#include <auralis/session/VolumeCoordinator.h>

#include <auralis/audio/AudioRouter.h>

#include <algorithm>
#include <cmath>

namespace auralis::session {

VolumeCoordinator::VolumeCoordinator(auralis::audio::AudioRouter* router)
    : router_(router)
{
}

double VolumeCoordinator::effectiveVolume(const AuralisSession& session, const SessionDevice& device)
{
    const double raw = session.groupVolume * device.volumeTrim;
    if (!std::isfinite(raw)) {
        return 0.0;
    }
    return std::clamp(raw, 0.0, 1.0);
}

void VolumeCoordinator::applyMemberVolume(const AuralisSession& session, const SessionDevice& device)
{
    if (router_ == nullptr || device.runtime.endpointId.isEmpty()) {
        return;
    }
    const double volume = effectiveVolume(session, device);
    const bool muted = session.muted || device.muted;
    router_->setDestinationVolume(device.runtime.endpointId, volume);
    router_->setDestinationMuted(device.runtime.endpointId, muted);
    router_->setDestinationDelayMs(device.runtime.endpointId, device.delayMs);
}

void VolumeCoordinator::applySessionVolumes(const AuralisSession& session)
{
    for (const SessionDevice& device : session.devices) {
        if (!device.enabled || !device.runtime.routeActive || device.runtime.endpointId.isEmpty()) {
            continue;
        }
        applyMemberVolume(session, device);
    }
    if (router_ != nullptr && !session.id.isEmpty()) {
        router_->syncSessionDestinations(session.id);
    }
}

} // namespace auralis::session

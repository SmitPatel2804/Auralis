#pragma once

#include <auralis/session/AuralisSession.h>

#include <QString>

namespace auralis::audio {
class AudioRouter;
}

namespace auralis::session {

class VolumeCoordinator {
public:
    explicit VolumeCoordinator(auralis::audio::AudioRouter* router);

    void applyMemberVolume(const AuralisSession& session, const SessionDevice& device);
    void applySessionVolumes(const AuralisSession& session);
    [[nodiscard]] static double effectiveVolume(const AuralisSession& session, const SessionDevice& device);

private:
    auralis::audio::AudioRouter* router_ = nullptr;
};

} // namespace auralis::session

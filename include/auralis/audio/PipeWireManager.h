#pragma once

#include <auralis/audio/IPipeWireManager.h>

namespace auralis::audio {

// Phase 1 foundation stub. Native PipeWire registry integration begins in Phase 4.
class PipeWireManager final : public IPipeWireManager {
public:
    bool initialize() override;
    void shutdown() override;
    auralis::core::ServiceStatus status() const noexcept override;

private:
    auralis::core::ServiceStatus status_ = auralis::core::ServiceStatus::Uninitialized;
};

} // namespace auralis::audio

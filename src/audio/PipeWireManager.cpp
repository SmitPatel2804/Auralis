#include <auralis/audio/PipeWireManager.h>

#include <auralis/core/LoggingCategories.h>

namespace auralis::audio {

bool PipeWireManager::initialize()
{
    // Phase 1 foundation stub. Native PipeWire registry integration begins in Phase 4.
    // Ready means this service object is initialized, not that a live graph was inspected.
    status_ = auralis::core::ServiceStatus::Initializing;
    status_ = auralis::core::ServiceStatus::Ready;
    qCInfo(auralisAudio) << "PipeWire manager skeleton ready";
    return true;
}

void PipeWireManager::shutdown()
{
    if (status_ == auralis::core::ServiceStatus::Uninitialized) {
        return;
    }

    qCInfo(auralisAudio) << "PipeWire manager skeleton shut down";
    status_ = auralis::core::ServiceStatus::Uninitialized;
}

auralis::core::ServiceStatus PipeWireManager::status() const noexcept
{
    return status_;
}

} // namespace auralis::audio

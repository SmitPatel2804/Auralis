#pragma once

#include <auralis/audio/IAudioManager.h>

namespace auralis::audio {

// Compatibility alias retained while downstream code and persisted diagnostics
// migrate from the Linux implementation name to the platform-neutral contract.
using IPipeWireManager = IAudioManager;

} // namespace auralis::audio

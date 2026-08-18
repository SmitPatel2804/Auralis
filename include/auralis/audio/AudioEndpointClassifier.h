#pragma once

#include <auralis/audio/AudioEndpoint.h>
#include <auralis/audio/PipeWireTypes.h>

#include <optional>

namespace auralis::audio {

class PipeWireObjectStore;

std::optional<AudioEndpoint> classifyAudioEndpoint(
    const PipeWireNodeInfo& node,
    const PipeWireObjectStore& store);

QString makeEndpointLogicalId(const AudioEndpoint& endpoint, const PipeWireNodeInfo& node);

} // namespace auralis::audio

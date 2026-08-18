#pragma once

#include <auralis/audio/AudioEndpoint.h>
#include <auralis/audio/PipeWireObjectStore.h>

namespace auralis::bluetooth {
class DeviceRegistry;
}

namespace auralis::audio {

class EndpointResolver {
public:
    explicit EndpointResolver(const bluetooth::DeviceRegistry* bluetoothRegistry = nullptr);

    void setBluetoothRegistry(const bluetooth::DeviceRegistry* bluetoothRegistry);
    void applyMapping(AudioEndpoint& endpoint, const PipeWireObjectStore& store) const;

private:
    const bluetooth::DeviceRegistry* bluetoothRegistry_ = nullptr;
};

void refreshEndpointsFromStore(
    const PipeWireObjectStore& store,
    class AudioEndpointRegistry& registry,
    const EndpointResolver& resolver);

} // namespace auralis::audio

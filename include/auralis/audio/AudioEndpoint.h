#pragma once

#include <QString>

#include <cstdint>
#include <optional>

namespace auralis::audio {

enum class AudioEndpointDirection {
    Playback,
    Capture,
    Duplex,
    Unknown
};

enum class AudioEndpointAvailability {
    Available,
    Unavailable
};

enum class AudioTransport {
    Unknown,
    BuiltIn,
    Alsa,
    BluetoothClassic,
    BluetoothLE
};

enum class EndpointMappingConfidence {
    None,
    Weak,
    Strong,
    Exact
};

enum class EndpointMappingReason {
    None,
    ExactBluetoothAddress,
    ExactBlueZObjectPath,
    PipeWireDeviceOwnership,
    UniqueNameFallback,
    Ambiguous,
    InsufficientData
};

struct AudioEndpoint {
    QString id;
    quint32 pipeWireObjectId = 0;
    std::optional<quint64> pipeWireSerial;
    QString name;
    QString description;
    AudioEndpointDirection direction = AudioEndpointDirection::Unknown;
    QString mediaClass;
    std::optional<quint32> pipeWireDeviceId;
    QString nodeName;
    QString profile;
    QString codec;
    std::optional<quint32> sampleRate;
    std::optional<quint32> channelCount;
    AudioEndpointAvailability availability = AudioEndpointAvailability::Available;
    AudioTransport transport = AudioTransport::Unknown;
    QString bluetoothDeviceId;
    QString bluetoothAddress;
    QString bluetoothDisplayName;
    QString bluezObjectPath;
    EndpointMappingConfidence mappingConfidence = EndpointMappingConfidence::None;
    EndpointMappingReason mappingReason = EndpointMappingReason::None;

    bool mapped() const noexcept { return !bluetoothDeviceId.isEmpty(); }
};

QString toString(AudioEndpointDirection direction);
QString toString(AudioEndpointAvailability availability);
QString toString(AudioTransport transport);
QString toString(EndpointMappingConfidence confidence);
QString toString(EndpointMappingReason reason);

bool endpointPublicStateEqual(const AudioEndpoint& left, const AudioEndpoint& right);

} // namespace auralis::audio

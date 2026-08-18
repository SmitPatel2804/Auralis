#include <auralis/audio/AudioEndpoint.h>

namespace auralis::audio {

QString toString(AudioEndpointDirection direction)
{
    switch (direction) {
    case AudioEndpointDirection::Playback:
        return QStringLiteral("Playback");
    case AudioEndpointDirection::Capture:
        return QStringLiteral("Capture");
    case AudioEndpointDirection::Duplex:
        return QStringLiteral("Duplex");
    case AudioEndpointDirection::Unknown:
        return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

QString toString(AudioEndpointAvailability availability)
{
    switch (availability) {
    case AudioEndpointAvailability::Available:
        return QStringLiteral("Available");
    case AudioEndpointAvailability::Unavailable:
        return QStringLiteral("Unavailable");
    }
    return QStringLiteral("Unknown");
}

QString toString(AudioTransport transport)
{
    switch (transport) {
    case AudioTransport::Unknown:
        return QStringLiteral("Unknown");
    case AudioTransport::BuiltIn:
        return QStringLiteral("Built-in");
    case AudioTransport::Alsa:
        return QStringLiteral("ALSA");
    case AudioTransport::BluetoothClassic:
        return QStringLiteral("Bluetooth");
    case AudioTransport::BluetoothLE:
        return QStringLiteral("Bluetooth LE");
    }
    return QStringLiteral("Unknown");
}

QString toString(EndpointMappingConfidence confidence)
{
    switch (confidence) {
    case EndpointMappingConfidence::None:
        return QStringLiteral("None");
    case EndpointMappingConfidence::Weak:
        return QStringLiteral("Weak");
    case EndpointMappingConfidence::Strong:
        return QStringLiteral("Strong");
    case EndpointMappingConfidence::Exact:
        return QStringLiteral("Exact");
    }
    return QStringLiteral("None");
}

QString toString(EndpointMappingReason reason)
{
    switch (reason) {
    case EndpointMappingReason::None:
        return QStringLiteral("None");
    case EndpointMappingReason::ExactBluetoothAddress:
        return QStringLiteral("ExactBluetoothAddress");
    case EndpointMappingReason::ExactBlueZObjectPath:
        return QStringLiteral("ExactBlueZObjectPath");
    case EndpointMappingReason::PipeWireDeviceOwnership:
        return QStringLiteral("PipeWireDeviceOwnership");
    case EndpointMappingReason::UniqueNameFallback:
        return QStringLiteral("UniqueNameFallback");
    case EndpointMappingReason::Ambiguous:
        return QStringLiteral("Ambiguous");
    case EndpointMappingReason::InsufficientData:
        return QStringLiteral("InsufficientData");
    }
    return QStringLiteral("None");
}

bool endpointPublicStateEqual(const AudioEndpoint& left, const AudioEndpoint& right)
{
    return left.id == right.id && left.pipeWireObjectId == right.pipeWireObjectId
        && left.pipeWireSerial == right.pipeWireSerial && left.name == right.name
        && left.description == right.description && left.direction == right.direction
        && left.mediaClass == right.mediaClass && left.pipeWireDeviceId == right.pipeWireDeviceId
        && left.nodeName == right.nodeName && left.profile == right.profile && left.codec == right.codec
        && left.sampleRate == right.sampleRate && left.channelCount == right.channelCount
        && left.availability == right.availability && left.transport == right.transport
        && left.bluetoothDeviceId == right.bluetoothDeviceId && left.bluetoothAddress == right.bluetoothAddress
        && left.bluetoothDisplayName == right.bluetoothDisplayName && left.bluezObjectPath == right.bluezObjectPath
        && left.mappingConfidence == right.mappingConfidence && left.mappingReason == right.mappingReason;
}

} // namespace auralis::audio

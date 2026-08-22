#include <auralis/audio/AudioEndpointClassifier.h>

#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/audio/PipeWireVirtualOutput.h>

namespace auralis::audio {
namespace {

bool isMonitorNode(const PipeWireNodeInfo& node)
{
    if (node.properties.boolValue(QStringLiteral("stream.monitor")).value_or(false)) {
        return true;
    }
    if (node.mediaClass.contains(QLatin1String("Monitor"), Qt::CaseInsensitive)) {
        return true;
    }
    return node.name.endsWith(QLatin1String(".monitor"));
}

AudioEndpointDirection directionFromMediaClass(const QString& mediaClass)
{
    if (mediaClass == QLatin1String("Audio/Sink")) {
        return AudioEndpointDirection::Playback;
    }
    if (mediaClass == QLatin1String("Audio/Source")) {
        return AudioEndpointDirection::Capture;
    }
    if (mediaClass == QLatin1String("Audio/Duplex")) {
        return AudioEndpointDirection::Duplex;
    }
    return AudioEndpointDirection::Unknown;
}

AudioTransport inferTransport(const PipeWireNodeInfo& node, const PipeWireDeviceInfo* device)
{
    const QString api = !node.properties.value(QStringLiteral("device.api")).isEmpty()
        ? node.properties.value(QStringLiteral("device.api"))
        : (device != nullptr ? device->api : QString());
    const QString lowered = api.toLower();
    if (lowered.contains(QLatin1String("bluez"))) {
        const QString profile = node.bluezProfile.value_or(device != nullptr ? device->bluezProfile.value_or(QString()) : QString())
                                    .toLower();
        if (profile.contains(QLatin1String("bap")) || profile.contains(QLatin1String("le"))) {
            return AudioTransport::BluetoothLE;
        }
        return AudioTransport::BluetoothClassic;
    }
    if (lowered.contains(QLatin1String("alsa"))) {
        const QString bus = !node.properties.value(QStringLiteral("device.bus")).isEmpty()
            ? node.properties.value(QStringLiteral("device.bus"))
            : (device != nullptr ? device->bus : QString());
        if (bus.compare(QLatin1String("pci"), Qt::CaseInsensitive) == 0) {
            return AudioTransport::BuiltIn;
        }
        return AudioTransport::Alsa;
    }
    if (node.bluezAddress.has_value() || node.bluezPath.has_value()
        || (device != nullptr && (device->bluezAddress.has_value() || device->bluezPath.has_value()))) {
        return AudioTransport::BluetoothClassic;
    }
    return AudioTransport::Unknown;
}

QString fallbackName(const PipeWireNodeInfo& node, const PipeWireDeviceInfo* device)
{
    if (!node.description.isEmpty()) {
        return node.description;
    }
    if (!node.nick.isEmpty()) {
        return node.nick;
    }
    if (!node.name.isEmpty()) {
        return node.name;
    }
    if (device != nullptr) {
        if (!device->description.isEmpty()) {
            return device->description;
        }
        if (!device->nick.isEmpty()) {
            return device->nick;
        }
        if (!device->name.isEmpty()) {
            return device->name;
        }
    }
    return QStringLiteral("Audio endpoint %1").arg(node.globalId);
}

} // namespace

std::optional<AudioEndpoint> classifyAudioEndpoint(
    const PipeWireNodeInfo& node,
    const PipeWireObjectStore& store)
{
    if (node.mediaClass.startsWith(QLatin1String("Stream/")) || node.mediaClass.startsWith(QLatin1String("Video/"))) {
        return std::nullopt;
    }
    if (isMonitorNode(node)) {
        return std::nullopt;
    }
    // Never offer the Auralis input sink as one of its own playback
    // destinations. That would form a graph feedback loop.
    if (isAuralisPipeWireVirtualSink(node)) {
        return std::nullopt;
    }
    const AudioEndpointDirection direction = directionFromMediaClass(node.mediaClass);
    if (direction == AudioEndpointDirection::Unknown) {
        return std::nullopt;
    }

    const PipeWireDeviceInfo* device = node.deviceId.has_value() ? store.device(*node.deviceId) : nullptr;
    AudioEndpoint endpoint;
    endpoint.pipeWireObjectId = node.globalId;
    endpoint.pipeWireSerial = node.serial;
    endpoint.pipeWireDeviceId = node.deviceId;
    endpoint.nodeName = node.name;
    endpoint.mediaClass = node.mediaClass;
    endpoint.direction = direction;
    endpoint.availability = AudioEndpointAvailability::Available;
    endpoint.transport = inferTransport(node, device);
    endpoint.name = fallbackName(node, device);
    endpoint.description = !node.description.isEmpty() ? node.description : endpoint.name;
    endpoint.profile = node.bluezProfile.value_or(
        device != nullptr ? device->bluezProfile.value_or(QString()) : QString());
    endpoint.codec = node.bluezCodec.value_or(device != nullptr ? device->bluezCodec.value_or(QString()) : QString());
    endpoint.sampleRate = node.sampleRate;
    endpoint.channelCount = node.channelCount;
    endpoint.bluetoothAddress = node.bluezAddress.value_or(QString());
    endpoint.bluezObjectPath = node.bluezPath.value_or(QString());
    endpoint.id = makeEndpointLogicalId(endpoint, node);
    return endpoint;
}

QString makeEndpointLogicalId(const AudioEndpoint& endpoint, const PipeWireNodeInfo& node)
{
    const QString direction = toString(endpoint.direction).toLower();
    const QString profileOrNode = !endpoint.profile.isEmpty() ? endpoint.profile : node.name;
    if (!endpoint.bluetoothAddress.isEmpty() || endpoint.transport == AudioTransport::BluetoothClassic
        || endpoint.transport == AudioTransport::BluetoothLE) {
        const QString address = !endpoint.bluetoothAddress.isEmpty() ? endpoint.bluetoothAddress : QStringLiteral("unknown");
        return QStringLiteral("bt:%1:%2:%3").arg(address, direction, profileOrNode);
    }
    const QString stable = node.serial.has_value()
        ? QString::number(*node.serial)
        : (!node.name.isEmpty() ? node.name : QString::number(node.globalId));
    return QStringLiteral("pw:%1:%2:%3").arg(stable, direction, profileOrNode);
}

} // namespace auralis::audio

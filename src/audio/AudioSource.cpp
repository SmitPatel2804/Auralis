#include <auralis/audio/AudioSource.h>

#include <auralis/audio/PipeWireObjectStore.h>

namespace auralis::audio {
namespace {

bool isRoutableAudioPort(const PipeWirePortInfo& port)
{
    if (port.control) {
        return false;
    }
    const QString lowered = port.name.toLower();
    if (lowered.contains(QLatin1String("midi")) || lowered.contains(QLatin1String("control"))) {
        return false;
    }
    return true;
}

bool isVirtualSource(const PipeWireNodeInfo& node)
{
    if (node.properties.boolValue(QStringLiteral("node.virtual")).value_or(false)) {
        return true;
    }
    const QString factory = node.properties.value(QStringLiteral("factory.name")).toLower();
    return factory.contains(QLatin1String("adapter")) && node.applicationName.isEmpty()
        && node.properties.value(QStringLiteral("device.api")).isEmpty();
}

AudioSource makeSourceFromNode(const PipeWireNodeInfo& node, AudioSourceType type, bool monitor)
{
    AudioSource source;
    source.pipeWireNodeId = node.globalId;
    source.objectSerial = node.serial;
    source.nodeName = node.name;
    source.description = !node.description.isEmpty() ? node.description : node.name;
    source.mediaClass = node.mediaClass;
    source.sourceType = type;
    source.available = true;
    source.applicationName = node.applicationName;
    source.processId = node.processId;
    source.monitorSource = monitor;
    source.id = makeSourceLogicalId(source);
    return source;
}

} // namespace

QString toString(AudioSourceType type)
{
    switch (type) {
    case AudioSourceType::ApplicationPlaybackStream:
        return QStringLiteral("ApplicationPlaybackStream");
    case AudioSourceType::PhysicalAudioSource:
        return QStringLiteral("PhysicalAudioSource");
    case AudioSourceType::VirtualAudioSource:
        return QStringLiteral("VirtualAudioSource");
    case AudioSourceType::SinkMonitor:
        return QStringLiteral("SinkMonitor");
    case AudioSourceType::Unknown:
        return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

QString makeSourceLogicalId(const AudioSource& source)
{
    const QString token = source.objectSerial.has_value()
        ? QString::number(*source.objectSerial)
        : (!source.nodeName.isEmpty() ? source.nodeName : QString::number(source.pipeWireNodeId));
    if (source.monitorSource) {
        return QStringLiteral("src:%1:SinkMonitor").arg(token);
    }
    const QString media = !source.mediaClass.isEmpty() ? source.mediaClass : QStringLiteral("unknown");
    return QStringLiteral("src:%1:%2").arg(token, media);
}

std::optional<AudioSource> classifyAudioSource(const PipeWireNodeInfo& node, const PipeWireObjectStore& store)
{
    if (node.mediaClass.startsWith(QLatin1String("Video/")) || node.mediaClass.startsWith(QLatin1String("Midi/"))) {
        return std::nullopt;
    }
    if (node.mediaClass.startsWith(QLatin1String("Stream/Input/"))) {
        return std::nullopt;
    }

    if (node.mediaClass == QLatin1String("Stream/Output/Audio")) {
        AudioSource source = makeSourceFromNode(node, AudioSourceType::ApplicationPlaybackStream, false);
        for (const PipeWirePortInfo& port : store.portsForNode(node.globalId)) {
            if (port.direction == PipeWirePortDirection::Output && isRoutableAudioPort(port)) {
                source.portIds.push_back(port.globalId);
            }
        }
        if (source.portIds.isEmpty()) {
            return std::nullopt;
        }
        source.id = makeSourceLogicalId(source);
        return source;
    }

    if (node.mediaClass == QLatin1String("Audio/Source")) {
        const AudioSourceType type =
            isVirtualSource(node) ? AudioSourceType::VirtualAudioSource : AudioSourceType::PhysicalAudioSource;
        AudioSource source = makeSourceFromNode(node, type, false);
        for (const PipeWirePortInfo& port : store.portsForNode(node.globalId)) {
            if (port.direction == PipeWirePortDirection::Output && isRoutableAudioPort(port) && !port.monitor) {
                source.portIds.push_back(port.globalId);
            }
        }
        if (source.portIds.isEmpty()) {
            return std::nullopt;
        }
        source.id = makeSourceLogicalId(source);
        return source;
    }

    if (node.mediaClass == QLatin1String("Audio/Sink")) {
        AudioSource source = makeSourceFromNode(node, AudioSourceType::SinkMonitor, true);
        for (const PipeWirePortInfo& port : store.portsForNode(node.globalId)) {
            if (port.direction == PipeWirePortDirection::Output && port.monitor && isRoutableAudioPort(port)) {
                source.portIds.push_back(port.globalId);
            }
        }
        if (source.portIds.isEmpty()) {
            return std::nullopt;
        }
        source.id = makeSourceLogicalId(source);
        return source;
    }

    return std::nullopt;
}

QVector<AudioSource> classifyAudioSources(const PipeWireObjectStore& store)
{
    QVector<AudioSource> result;
    for (const PipeWireNodeInfo& node : store.nodes()) {
        if (auto source = classifyAudioSource(node, store)) {
            result.push_back(*source);
        }
    }
    return result;
}

} // namespace auralis::audio

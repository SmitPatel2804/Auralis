#include <auralis/audio/AudioSource.h>

#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/audio/PipeWireVirtualOutput.h>

#include <QSet>

#include <algorithm>

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
    const QString mediaName = node.properties.value(QStringLiteral("media.name")).trimmed();
    if (!node.description.isEmpty()) {
        source.description = node.description;
    } else if (!mediaName.isEmpty()) {
        source.description = mediaName;
    } else {
        source.description = node.name;
    }
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

QString sourceListDisplayName(const AudioSource& source)
{
    if (!source.applicationName.isEmpty()) {
        if (!source.description.isEmpty() && source.description != source.applicationName
            && source.description != source.nodeName) {
            return source.applicationName + QStringLiteral(" — ") + source.description;
        }
        return source.applicationName;
    }
    const QString base = !source.description.isEmpty() ? source.description
        : (!source.nodeName.isEmpty() ? source.nodeName : source.id);
    if (source.monitorSource) {
        return base + QStringLiteral(" (monitor)");
    }
    if (source.id == QLatin1String(kAuralisVirtualSourceId)) {
        return base;
    }
    if (source.sourceType == AudioSourceType::PhysicalAudioSource
        || source.sourceType == AudioSourceType::VirtualAudioSource) {
        return base + QStringLiteral(" (mic)");
    }
    return base;
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

    // The sink half is selected by desktop applications. Only the source half
    // is routed by Auralis; exposing the sink monitor as well would create two
    // indistinguishable copies and make feedback selection possible.
    if (isAuralisPipeWireVirtualSink(node) || isAuralisInternalGraphNode(node)) {
        return std::nullopt;
    }

    if (isAuralisPipeWireVirtualSource(node)) {
        AudioSource source = makeSourceFromNode(node, AudioSourceType::VirtualAudioSource, false);
        source.id = QString::fromLatin1(kAuralisVirtualSourceId);
        source.description = QStringLiteral("Auralis System Audio");
        source.applicationName.clear();
        for (const PipeWirePortInfo& port : store.portsForNode(node.globalId)) {
            if (port.direction == PipeWirePortDirection::Output && isRoutableAudioPort(port)) {
                source.portIds.push_back(port.globalId);
            }
        }
        if (source.portIds.isEmpty()) {
            return std::nullopt;
        }
        return source;
    }

    if (node.mediaClass == QLatin1String("Stream/Output/Audio")) {
        if (isAuralisInternalGraphNode(node)) {
            return std::nullopt;
        }
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
    QSet<quint32> defaultNodes;
    for (const PipeWireNodeInfo& node : store.nodes()) {
        if (node.properties.boolValue(QStringLiteral("device.default")).value_or(false)) {
            defaultNodes.insert(node.globalId);
        }
        if (auto source = classifyAudioSource(node, store)) {
            result.push_back(*source);
        }
    }
    // App playback first (what sessions usually want), then mics, then sink monitors.
    // Within a source type, keep the operating system's default device first.
    std::sort(result.begin(), result.end(), [&defaultNodes](const AudioSource& left, const AudioSource& right) {
        auto rank = [](AudioSourceType type) {
            switch (type) {
            case AudioSourceType::VirtualAudioSource:
                return 0;
            case AudioSourceType::ApplicationPlaybackStream:
                return 1;
            case AudioSourceType::PhysicalAudioSource:
                return 2;
            case AudioSourceType::SinkMonitor:
                return 3;
            case AudioSourceType::Unknown:
                return 4;
            }
            return 4;
        };
        const int leftRank = rank(left.sourceType);
        const int rightRank = rank(right.sourceType);
        if (leftRank != rightRank) {
            return leftRank < rightRank;
        }
        const bool leftIsDefault = defaultNodes.contains(left.pipeWireNodeId);
        const bool rightIsDefault = defaultNodes.contains(right.pipeWireNodeId);
        if (leftIsDefault != rightIsDefault) {
            return leftIsDefault;
        }
        return left.description < right.description;
    });
    return result;
}

} // namespace auralis::audio

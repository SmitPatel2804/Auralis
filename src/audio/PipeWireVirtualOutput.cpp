#include <auralis/audio/PipeWireVirtualOutput.h>

#include <auralis/audio/PipeWireObjectStore.h>

#include <QJsonDocument>
#include <QJsonObject>

namespace auralis::audio {
namespace {

bool isMarked(const PipeWireNodeInfo& node, QStringView role)
{
    return node.properties.boolValue(QStringLiteral("auralis.virtual.output")).value_or(false)
        && node.properties.value(QStringLiteral("auralis.virtual.role")) == role;
}

bool hasAudioPort(
    const PipeWireObjectStore& store,
    quint32 nodeId,
    PipeWirePortDirection direction)
{
    for (const PipeWirePortInfo& port : store.portsForNode(nodeId)) {
        if (port.direction != direction || port.control) {
            continue;
        }
        const QString name = port.name.toLower();
        if (!name.contains(QLatin1String("midi")) && !name.contains(QLatin1String("control"))) {
            return true;
        }
    }
    return false;
}

} // namespace

bool isAuralisPipeWireVirtualSink(const PipeWireNodeInfo& node)
{
    return node.mediaClass == QLatin1String("Audio/Sink")
        && (isMarked(node, QStringLiteral("sink"))
            || node.name == QLatin1String(kAuralisVirtualSinkNodeName));
}

bool isAuralisPipeWireVirtualSource(const PipeWireNodeInfo& node)
{
    return node.mediaClass == QLatin1String("Stream/Output/Audio")
        && (isMarked(node, QStringLiteral("source"))
            || node.name == QLatin1String(kAuralisVirtualSourceNodeName));
}

PipeWireVirtualOutputState inspectPipeWireVirtualOutput(
    const PipeWireObjectStore& store,
    QStringView defaultAudioSinkName)
{
    PipeWireVirtualOutputState result;
    for (const PipeWireNodeInfo& node : store.nodes()) {
        if (isAuralisPipeWireVirtualSink(node)) {
            result.sinkNodeId = node.globalId;
            result.sinkNodeName = node.name;
            result.sinkReady = hasAudioPort(store, node.globalId, PipeWirePortDirection::Input);
        } else if (isAuralisPipeWireVirtualSource(node)) {
            result.sourceNodeId = node.globalId;
            result.sourceReady = hasAudioPort(store, node.globalId, PipeWirePortDirection::Output);
        } else {
            continue;
        }

        const QString persistence = node.properties.value(QStringLiteral("auralis.virtual.persistence"));
        result.packageManaged = result.packageManaged || persistence == QLatin1String("package");
        result.runtimeManaged = result.runtimeManaged || persistence == QLatin1String("runtime");
    }

    result.selectedAsDefault = result.sinkNodeId != 0 && !defaultAudioSinkName.isEmpty()
        && result.sinkNodeName == defaultAudioSinkName;
    return result;
}

QString pipeWireDefaultNodeName(QStringView metadataJson)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(metadataJson.toString().toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object().value(QStringLiteral("name")).toString().trimmed();
}

QByteArray pipeWireVirtualOutputModuleArguments()
{
    return QByteArrayLiteral(R"PW({
        node.name = "auralis_virtual_output"
        node.description = "Auralis Virtual Output"
        audio.rate = 48000
        audio.channels = 2
        audio.position = [ FL FR ]
        target.delay.sec = 0.0
        capture.props = {
            node.name = "auralis_virtual_output"
            node.description = "Auralis Virtual Output"
            media.class = "Audio/Sink"
            node.virtual = true
            node.autoconnect = false
            priority.session = 100
            auralis.virtual.output = true
            auralis.virtual.role = "sink"
            auralis.virtual.persistence = "runtime"
        }
        playback.props = {
            node.name = "auralis_virtual_output.source"
            node.description = "Auralis System Audio"
            media.class = "Stream/Output/Audio"
            node.virtual = true
            node.autoconnect = false
            node.dont-fallback = true
            node.dont-reconnect = true
            node.passive = true
            stream.dont-remix = true
            auralis.virtual.output = true
            auralis.virtual.role = "source"
            auralis.virtual.persistence = "runtime"
        }
    })PW");
}

} // namespace auralis::audio

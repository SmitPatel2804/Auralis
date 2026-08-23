#include <auralis/audio/PipeWireVirtualOutput.h>

#include <auralis/audio/PipeWireObjectStore.h>

#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

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
            node.nick = "Auralis Virtual Output"
            node.virtual = false
            node.autoconnect = false
            session.suspend-timeout-seconds = 0
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

bool isAuralisSessionFanoutSink(const PipeWireNodeInfo& node)
{
    return node.mediaClass == QLatin1String("Audio/Sink")
        && (node.name == QLatin1String(kAuralisSessionFanoutNodeName)
            || node.properties.boolValue(QStringLiteral("auralis.session.fanout")).value_or(false));
}

bool isAuralisDelayBridgeNode(const PipeWireNodeInfo& node)
{
    if (node.properties.boolValue(QStringLiteral("auralis.delay.bridge")).value_or(false)) {
        return true;
    }
    if (node.name.startsWith(QLatin1String("auralis_delay_"))) {
        return true;
    }
    return node.name.contains(QLatin1String(".auralis_delay_"));
}

bool isAuralisInternalGraphNode(const PipeWireNodeInfo& node)
{
    if (node.properties.boolValue(QStringLiteral("auralis.session.fanout")).value_or(false)) {
        return true;
    }
    return isAuralisPipeWireVirtualSink(node) || isAuralisSessionFanoutSink(node) || isAuralisDelayBridgeNode(node);
}

namespace {

QString sanitizedGraphToken(const QString& raw)
{
    QString token;
    token.reserve(raw.size());
    for (const QChar ch : raw) {
        if (ch.isLetterOrNumber()) {
            token += ch;
        } else {
            token += QLatin1Char('_');
        }
    }
    token = token.left(48);
    return token.isEmpty() ? QStringLiteral("dest") : token;
}

bool isSafePipeWireName(const QString& name)
{
    return !name.isEmpty() && !name.contains(QLatin1Char('"')) && !name.contains(QLatin1Char('\\'))
        && !name.contains(QLatin1Char('{')) && !name.contains(QLatin1Char('}'));
}

} // namespace

QString auralisDelayBridgeCaptureNodeName(const QString& endpointId)
{
    return QStringLiteral("auralis_delay_") + sanitizedGraphToken(endpointId);
}

QString auralisDelayBridgePlaybackNodeName(const QString& endpointId)
{
    return auralisDelayBridgeCaptureNodeName(endpointId) + QStringLiteral(".source");
}

QByteArray pipeWireSessionFanoutModuleArguments(const QStringList& sinkNodeNames)
{
    QByteArray rules;
    for (const QString& name : sinkNodeNames) {
        if (!isSafePipeWireName(name) || name == QLatin1String(kAuralisVirtualSinkNodeName)
            || name == QLatin1String(kAuralisSessionFanoutNodeName)) {
            continue;
        }
        rules += "                    {\n"
                 "                        matches = [ { node.name = \"";
        rules += name.toUtf8();
        rules += "\" } ]\n"
                 "                        actions = { create-stream = { } }\n"
                 "                    }\n";
    }
    QByteArray args = QByteArrayLiteral(R"PW({
        combine.mode = sink
        combine.latency-compensate = true
        node.name = "auralis_session_fanout"
        node.description = "Auralis Session Mix"
        audio.rate = 48000
        audio.channels = 2
        audio.position = [ FL FR ]
        combine.props = {
            node.name = "auralis_session_fanout"
            node.description = "Auralis Session Mix"
            media.class = "Audio/Sink"
            node.virtual = true
            node.autoconnect = false
            session.suspend-timeout-seconds = 0
            auralis.session.fanout = true
        }
        stream.props = {
            node.virtual = true
            node.dont-fallback = true
            auralis.session.fanout = true
        }
        stream.rules = [
)PW");
    args += rules;
    args += "        ]\n    }";
    return args;
}

QByteArray pipeWireDelayBridgeModuleArguments(
    const QString& endpointId,
    const QString& destNodeName,
    double delaySeconds)
{
    const QString capture = auralisDelayBridgeCaptureNodeName(endpointId);
    const QString playback = capture + QStringLiteral(".source");
    const QString token = sanitizedGraphToken(endpointId);
    const double clamped = std::clamp(std::isfinite(delaySeconds) ? delaySeconds : 0.0, 0.0, 0.5);
    QByteArray args = QByteArrayLiteral("{\n"
        "        node.name = \"");
    args += capture.toUtf8();
    args += "\"\n        node.description = \"Auralis Delay\"\n"
            "        audio.rate = 48000\n        audio.channels = 2\n"
            "        audio.position = [ FL FR ]\n        target.delay.sec = ";
    args += QByteArray::number(clamped, 'f', 4);
    args += "\n        capture.props = {\n            node.name = \"";
    args += capture.toUtf8();
    args += "\"\n            node.description = \"Auralis Delay\"\n"
            "            media.class = \"Audio/Sink\"\n"
            "            node.virtual = false\n"
            "            node.autoconnect = false\n"
            "            session.suspend-timeout-seconds = 0\n"
            "            auralis.delay.bridge = true\n"
            "            auralis.delay.role = \"capture\"\n"
            "            auralis.delay.token = \"";
    args += token.toUtf8();
    args += "\"\n        }\n        playback.props = {\n            node.name = \"";
    args += playback.toUtf8();
    args += "\"\n            node.description = \"Auralis Delay\"\n"
            "            media.class = \"Stream/Output/Audio\"\n"
            "            node.virtual = true\n"
            "            node.autoconnect = true\n"
            "            node.dont-fallback = true\n"
            "            auralis.delay.bridge = true\n"
            "            auralis.delay.role = \"playback\"\n"
            "            auralis.delay.token = \"";
    args += token.toUtf8();
    args += "\"\n";
    if (isSafePipeWireName(destNodeName)) {
        args += "            target.object = \"";
        args += destNodeName.toUtf8();
        args += "\"\n";
    }
    args += "        }\n    }";
    return args;
}

} // namespace auralis::audio

#include <auralis/audio/RoutePlanner.h>

#include <auralis/audio/AudioEndpointRegistry.h>

#include <algorithm>

namespace auralis::audio {
namespace {

bool isAudioPort(const PipeWirePortInfo& port)
{
    if (port.control) {
        return false;
    }
    const QString lowered = port.name.toLower();
    return !lowered.contains(QLatin1String("midi")) && !lowered.contains(QLatin1String("control"));
}

QVector<PipeWirePortInfo> outputPorts(const AudioSource& source, const PipeWireObjectStore& store)
{
    QVector<PipeWirePortInfo> result;
    if (store.node(source.pipeWireNodeId) != nullptr) {
        for (const PipeWirePortInfo& port : store.portsForNode(source.pipeWireNodeId)) {
            if (port.direction == PipeWirePortDirection::Output && isAudioPort(port)
                && (!source.monitorSource || port.monitor)) {
                result.push_back(port);
            }
        }
    }
    if (result.isEmpty()) {
        for (quint32 id : source.portIds) {
            if (const PipeWirePortInfo* port = store.port(id)) {
                if (port->direction == PipeWirePortDirection::Output && isAudioPort(*port)) {
                    result.push_back(*port);
                }
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const PipeWirePortInfo& a, const PipeWirePortInfo& b) {
        return a.globalId < b.globalId;
    });
    return result;
}

QVector<PipeWirePortInfo> inputPorts(quint32 nodeId, const PipeWireObjectStore& store)
{
    QVector<PipeWirePortInfo> result;
    for (const PipeWirePortInfo& port : store.portsForNode(nodeId)) {
        if (port.direction == PipeWirePortDirection::Input && isAudioPort(port) && !port.monitor) {
            result.push_back(port);
        }
    }
    return result;
}

const AudioSource* findSource(const QVector<AudioSource>& sources, const QString& sourceId)
{
    for (const AudioSource& source : sources) {
        if (source.id == sourceId) {
            return &source;
        }
    }
    return nullptr;
}

bool hasChannelMetadata(const QVector<PipeWirePortInfo>& ports)
{
    for (const PipeWirePortInfo& port : ports) {
        if (!port.audioChannel.isEmpty()) {
            return true;
        }
    }
    return false;
}

const PipeWirePortInfo* findChannel(const QVector<PipeWirePortInfo>& ports, const QString& channel)
{
    for (const PipeWirePortInfo& port : ports) {
        if (port.audioChannel == channel) {
            return &port;
        }
    }
    return nullptr;
}

QVector<ResolvedPortPair> matchByIndex(
    const QVector<PipeWirePortInfo>& outputs,
    const QVector<PipeWirePortInfo>& inputs,
    const QString& destinationId)
{
    QVector<ResolvedPortPair> pairs;
    const int count = static_cast<int>(std::min(outputs.size(), inputs.size()));
    for (int i = 0; i < count; ++i) {
        ResolvedPortPair pair;
        pair.destinationId = destinationId;
        pair.outputNodeId = outputs.at(i).nodeId.value_or(0);
        pair.outputPortId = outputs.at(i).globalId;
        pair.inputNodeId = inputs.at(i).nodeId.value_or(0);
        pair.inputPortId = inputs.at(i).globalId;
        pair.channel = !outputs.at(i).audioChannel.isEmpty() ? outputs.at(i).audioChannel : QString::number(i);
        pairs.push_back(pair);
    }
    return pairs;
}

QVector<ResolvedPortPair> matchByChannel(
    const QVector<PipeWirePortInfo>& outputs,
    const QVector<PipeWirePortInfo>& inputs,
    const QString& destinationId)
{
    QVector<ResolvedPortPair> pairs;
    auto add = [&](const PipeWirePortInfo& out, const PipeWirePortInfo& in, const QString& channel) {
        ResolvedPortPair pair;
        pair.destinationId = destinationId;
        pair.outputNodeId = out.nodeId.value_or(0);
        pair.outputPortId = out.globalId;
        pair.inputNodeId = in.nodeId.value_or(0);
        pair.inputPortId = in.globalId;
        pair.channel = channel;
        pairs.push_back(pair);
    };

    const PipeWirePortInfo* outMono = findChannel(outputs, QStringLiteral("MONO"));
    const PipeWirePortInfo* inMono = findChannel(inputs, QStringLiteral("MONO"));
    if (outMono != nullptr && inMono != nullptr) {
        add(*outMono, *inMono, QStringLiteral("MONO"));
        return pairs;
    }

    const QStringList stereo{QStringLiteral("FL"), QStringLiteral("FR")};
    bool matchedStereo = false;
    for (const QString& channel : stereo) {
        const PipeWirePortInfo* out = findChannel(outputs, channel);
        const PipeWirePortInfo* in = findChannel(inputs, channel);
        if (out != nullptr && in != nullptr) {
            add(*out, *in, channel);
            matchedStereo = true;
        }
    }
    if (matchedStereo) {
        return pairs;
    }

    if (outMono != nullptr) {
        for (const PipeWirePortInfo& in : inputs) {
            if (in.audioChannel == QLatin1String("FL") || in.audioChannel == QLatin1String("FR")
                || in.audioChannel == QLatin1String("MONO")) {
                add(*outMono, in, in.audioChannel);
            }
        }
        if (!pairs.isEmpty()) {
            return pairs;
        }
    }

    if (inMono != nullptr) {
        const PipeWirePortInfo* outFl = findChannel(outputs, QStringLiteral("FL"));
        const PipeWirePortInfo* out = outFl != nullptr ? outFl : (outputs.isEmpty() ? nullptr : &outputs.front());
        if (out != nullptr) {
            add(*out, *inMono, QStringLiteral("MONO"));
            return pairs;
        }
    }

    return matchByIndex(outputs, inputs, destinationId);
}

} // namespace

ResolvedRoutePlan RoutePlanner::plan(
    const QString& sourceId,
    const QStringList& destinationEndpointIds,
    const PipeWireObjectStore& store,
    const AudioEndpointRegistry& endpoints) const
{
    ResolvedRoutePlan plan;
    plan.sourceId = sourceId;
    plan.destinationIds = destinationEndpointIds;

    if (sourceId.isEmpty()) {
        plan.error = {RouteError::SourceNotFound, QStringLiteral("Source id is empty")};
        return plan;
    }
    if (destinationEndpointIds.isEmpty()) {
        plan.error = {RouteError::DestinationNotFound, QStringLiteral("No destinations selected")};
        return plan;
    }

    QStringList uniqueDests;
    for (const QString& id : destinationEndpointIds) {
        if (!uniqueDests.contains(id)) {
            uniqueDests.push_back(id);
        }
    }
    plan.destinationIds = uniqueDests;

    const QVector<AudioSource> sources = classifyAudioSources(store);
    const AudioSource* source = findSource(sources, sourceId);
    if (source == nullptr) {
        plan.error = {RouteError::SourceNotFound, QStringLiteral("Source %1 is not currently routable").arg(sourceId)};
        return plan;
    }
    plan.sourceNodeId = source->pipeWireNodeId;
    if (uniqueDests.contains(sourceId)) {
        plan.error = {RouteError::UnsupportedDirection, QStringLiteral("Source cannot also be a destination")};
        return plan;
    }

    const QVector<PipeWirePortInfo> outputs = outputPorts(*source, store);
    if (outputs.isEmpty()) {
        plan.error = {RouteError::SourceNotRoutable, QStringLiteral("Source has no audio output ports")};
        return plan;
    }

    for (const QString& destId : uniqueDests) {
        const AudioEndpoint* endpoint = endpoints.findById(destId);
        if (endpoint == nullptr) {
            plan.error = {RouteError::DestinationNotFound, QStringLiteral("Destination %1 not found").arg(destId)};
            return plan;
        }
        if (endpoint->direction != AudioEndpointDirection::Playback
            && endpoint->direction != AudioEndpointDirection::Duplex) {
            plan.error = {
                RouteError::UnsupportedDirection,
                QStringLiteral("Destination %1 is not a playback endpoint").arg(destId)};
            return plan;
        }
        if (endpoint->availability != AudioEndpointAvailability::Available) {
            plan.error = {RouteError::DestinationUnavailable, QStringLiteral("Destination %1 is unavailable").arg(destId)};
            return plan;
        }
        if (store.node(endpoint->pipeWireObjectId) == nullptr) {
            plan.error = {
                RouteError::DestinationUnavailable,
                QStringLiteral("Destination %1 has no current PipeWire node").arg(destId)};
            return plan;
        }

        const QVector<PipeWirePortInfo> inputs = inputPorts(endpoint->pipeWireObjectId, store);
        if (inputs.isEmpty()) {
            plan.error = {RouteError::NoCompatiblePorts, QStringLiteral("Destination %1 has no audio input ports").arg(destId)};
            return plan;
        }

        QVector<ResolvedPortPair> pairs;
        if (hasChannelMetadata(outputs) || hasChannelMetadata(inputs)) {
            pairs = matchByChannel(outputs, inputs, destId);
        } else {
            pairs = matchByIndex(outputs, inputs, destId);
        }
        if (pairs.isEmpty()) {
            plan.error = {
                RouteError::NoCompatiblePorts,
                QStringLiteral("No compatible ports between source and %1").arg(destId)};
            return plan;
        }
        for (const ResolvedPortPair& pair : pairs) {
            plan.pairs.push_back(pair);
        }
    }

    return plan;
}

ResolvedRoutePlan RoutePlanner::planToSinkNode(
    const QString& sourceId,
    quint32 sinkNodeId,
    const QString& destinationId,
    const PipeWireObjectStore& store) const
{
    ResolvedRoutePlan plan;
    plan.sourceId = sourceId;
    if (!destinationId.isEmpty()) {
        plan.destinationIds = {destinationId};
    }

    if (sourceId.isEmpty()) {
        plan.error = {RouteError::SourceNotFound, QStringLiteral("Source id is empty")};
        return plan;
    }
    if (sinkNodeId == 0) {
        plan.error = {RouteError::DestinationUnavailable, QStringLiteral("Fanout sink is not available")};
        return plan;
    }

    const QVector<AudioSource> sources = classifyAudioSources(store);
    const AudioSource* source = findSource(sources, sourceId);
    if (source == nullptr) {
        plan.error = {RouteError::SourceNotFound, QStringLiteral("Source %1 is not currently routable").arg(sourceId)};
        return plan;
    }
    plan.sourceNodeId = source->pipeWireNodeId;

    const QVector<PipeWirePortInfo> outputs = outputPorts(*source, store);
    if (outputs.isEmpty()) {
        plan.error = {RouteError::SourceNotRoutable, QStringLiteral("Source has no audio output ports")};
        return plan;
    }
    if (store.node(sinkNodeId) == nullptr) {
        plan.error = {RouteError::DestinationUnavailable, QStringLiteral("Fanout sink has no current PipeWire node")};
        return plan;
    }

    const QVector<PipeWirePortInfo> inputs = inputPorts(sinkNodeId, store);
    if (inputs.isEmpty()) {
        plan.error = {RouteError::NoCompatiblePorts, QStringLiteral("Fanout sink has no audio input ports")};
        return plan;
    }

    QVector<ResolvedPortPair> pairs;
    if (hasChannelMetadata(outputs) || hasChannelMetadata(inputs)) {
        pairs = matchByChannel(outputs, inputs, destinationId);
    } else {
        pairs = matchByIndex(outputs, inputs, destinationId);
    }
    if (pairs.isEmpty()) {
        plan.error = {RouteError::NoCompatiblePorts, QStringLiteral("No compatible ports between source and fanout sink")};
        return plan;
    }
    plan.pairs = std::move(pairs);
    return plan;
}

} // namespace auralis::audio

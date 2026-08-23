#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace auralis::audio {

class PipeWireObjectStore;
struct PipeWireNodeInfo;

inline constexpr auto kAuralisVirtualSinkNodeName = "auralis_virtual_output";
inline constexpr auto kAuralisVirtualSourceNodeName = "auralis_virtual_output.source";
inline constexpr auto kAuralisVirtualSourceId = "src:auralis-system-audio";
inline constexpr auto kAuralisSessionFanoutNodeName = "auralis_session_fanout";

struct PipeWireVirtualOutputState {
    quint32 sinkNodeId = 0;
    quint32 sourceNodeId = 0;
    QString sinkNodeName;
    bool sinkReady = false;
    bool sourceReady = false;
    bool packageManaged = false;
    bool runtimeManaged = false;
    bool selectedAsDefault = false;

    bool ready() const noexcept { return sinkReady && sourceReady; }
    bool partial() const noexcept { return sinkNodeId != 0 || sourceNodeId != 0; }
};

bool isAuralisPipeWireVirtualSink(const PipeWireNodeInfo& node);
bool isAuralisPipeWireVirtualSource(const PipeWireNodeInfo& node);
PipeWireVirtualOutputState inspectPipeWireVirtualOutput(
    const PipeWireObjectStore& store,
    QStringView defaultAudioSinkName = {});

/// Parses PipeWire's Spa:String:JSON default-device metadata value.
QString pipeWireDefaultNodeName(QStringView metadataJson);

/// Arguments for the in-process fallback. Installed packages use the matching
/// PipeWire drop-in and therefore remain present after Auralis exits.
QByteArray pipeWireVirtualOutputModuleArguments();

bool isAuralisSessionFanoutSink(const PipeWireNodeInfo& node);
bool isAuralisDelayBridgeNode(const PipeWireNodeInfo& node);
bool isAuralisInternalGraphNode(const PipeWireNodeInfo& node);
QByteArray pipeWireSessionFanoutModuleArguments(const QStringList& sinkNodeNames);
QByteArray pipeWireDelayBridgeModuleArguments(
    const QString& endpointId,
    const QString& destNodeName,
    double delaySeconds);

} // namespace auralis::audio

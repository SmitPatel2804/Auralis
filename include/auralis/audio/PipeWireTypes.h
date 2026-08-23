#pragma once

#include <auralis/audio/PipeWireProperties.h>

#include <QString>

#include <cstdint>
#include <optional>

namespace auralis::audio {

enum class PipeWireConnectionState {
    Stopped,
    Starting,
    Connected,
    Error,
    Stopping
};

enum class PipeWireObjectKind {
    Unknown,
    Device,
    Node,
    Port,
    Link,
    Metadata,
    Other
};

struct PipeWireObjectSnapshot {
    quint32 globalId = 0;
    QString interfaceType;
    PipeWireObjectKind kind = PipeWireObjectKind::Unknown;
    PipeWireProperties properties;
};

struct PipeWireDeviceInfo {
    quint32 globalId = 0;
    std::optional<quint64> serial;
    QString name;
    QString description;
    QString nick;
    QString api;
    QString bus;
    std::optional<QString> bluezAddress;
    std::optional<QString> bluezPath;
    std::optional<QString> bluezProfile;
    std::optional<QString> bluezCodec;
    PipeWireProperties properties;

    bool operator==(const PipeWireDeviceInfo& other) const = default;
};

struct PipeWireNodeInfo {
    quint32 globalId = 0;
    std::optional<quint64> serial;
    std::optional<quint32> deviceId;
    QString name;
    QString description;
    QString nick;
    QString mediaClass;
    QString applicationName;
    std::optional<quint32> processId;
    std::optional<QString> bluezAddress;
    std::optional<QString> bluezPath;
    std::optional<QString> bluezProfile;
    std::optional<QString> bluezCodec;
    std::optional<quint32> channelCount;
    std::optional<quint32> sampleRate;
    std::optional<qint64> inputLatencyNs;
    PipeWireProperties properties;

    bool operator==(const PipeWireNodeInfo& other) const = default;
};

enum class PipeWirePortDirection {
    Unknown,
    Input,
    Output
};

enum class PipeWireLinkState {
    Unknown,
    Error,
    Unlinked,
    Init,
    Negotiating,
    Allocating,
    Paused,
    Active
};

struct PipeWirePortInfo {
    quint32 globalId = 0;
    std::optional<quint64> serial;
    std::optional<quint32> nodeId;
    PipeWirePortDirection direction = PipeWirePortDirection::Unknown;
    QString name;
    QString alias;
    QString audioChannel;
    bool monitor = false;
    bool physical = false;
    bool terminal = false;
    bool control = false;
    PipeWireProperties properties;

    bool operator==(const PipeWirePortInfo& other) const = default;
};

struct PipeWireLinkInfo {
    quint32 globalId = 0;
    std::optional<quint32> outputNode;
    std::optional<quint32> outputPort;
    std::optional<quint32> inputNode;
    std::optional<quint32> inputPort;
    PipeWireLinkState state = PipeWireLinkState::Unknown;
    QString error;
    PipeWireProperties properties;

    bool operator==(const PipeWireLinkInfo& other) const = default;
};

QString toString(PipeWireConnectionState state);
QString toString(PipeWirePortDirection direction);
QString toString(PipeWireLinkState state);
PipeWireObjectKind kindFromInterfaceType(const QString& interfaceType);
PipeWireDeviceInfo deviceInfoFromSnapshot(const PipeWireObjectSnapshot& snapshot);
PipeWireNodeInfo nodeInfoFromSnapshot(const PipeWireObjectSnapshot& snapshot);
PipeWirePortInfo portInfoFromSnapshot(const PipeWireObjectSnapshot& snapshot);
PipeWireLinkInfo linkInfoFromSnapshot(const PipeWireObjectSnapshot& snapshot);
void mergeDeviceInfo(PipeWireDeviceInfo& target, const PipeWireObjectSnapshot& snapshot);
void mergeNodeInfo(PipeWireNodeInfo& target, const PipeWireObjectSnapshot& snapshot);
void mergePortInfo(PipeWirePortInfo& target, const PipeWireObjectSnapshot& snapshot);
void mergeLinkInfo(PipeWireLinkInfo& target, const PipeWireObjectSnapshot& snapshot);
PipeWirePortDirection parsePortDirection(const QString& raw);
PipeWireLinkState parseLinkState(const QString& raw);
QString canonicalAudioChannel(const QString& raw);

} // namespace auralis::audio

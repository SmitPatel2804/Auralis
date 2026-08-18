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
};

struct PipeWireNodeInfo {
    quint32 globalId = 0;
    std::optional<quint64> serial;
    std::optional<quint32> deviceId;
    QString name;
    QString description;
    QString nick;
    QString mediaClass;
    std::optional<QString> bluezAddress;
    std::optional<QString> bluezPath;
    std::optional<QString> bluezProfile;
    std::optional<QString> bluezCodec;
    std::optional<quint32> channelCount;
    std::optional<quint32> sampleRate;
    PipeWireProperties properties;
};

QString toString(PipeWireConnectionState state);
PipeWireObjectKind kindFromInterfaceType(const QString& interfaceType);
PipeWireDeviceInfo deviceInfoFromSnapshot(const PipeWireObjectSnapshot& snapshot);
PipeWireNodeInfo nodeInfoFromSnapshot(const PipeWireObjectSnapshot& snapshot);
void mergeDeviceInfo(PipeWireDeviceInfo& target, const PipeWireObjectSnapshot& snapshot);
void mergeNodeInfo(PipeWireNodeInfo& target, const PipeWireObjectSnapshot& snapshot);

} // namespace auralis::audio

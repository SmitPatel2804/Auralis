#pragma once

#include <auralis/audio/PipeWireTypes.h>

#include <QString>
#include <QVector>

#include <cstdint>
#include <optional>

namespace auralis::audio {

class PipeWireObjectStore;

enum class AudioSourceType {
    Unknown,
    ApplicationPlaybackStream,
    PhysicalAudioSource,
    VirtualAudioSource,
    SinkMonitor
};

struct AudioSource {
    QString id;
    quint32 pipeWireNodeId = 0;
    std::optional<quint64> objectSerial;
    QString nodeName;
    QString description;
    QString mediaClass;
    AudioSourceType sourceType = AudioSourceType::Unknown;
    bool available = true;
    QString applicationName;
    std::optional<quint32> processId;
    bool monitorSource = false;
    QVector<quint32> portIds;

    bool operator==(const AudioSource& other) const = default;
};

QString toString(AudioSourceType type);
QString sourceListDisplayName(const AudioSource& source);
QString makeSourceLogicalId(const AudioSource& source);
std::optional<AudioSource> classifyAudioSource(const PipeWireNodeInfo& node, const PipeWireObjectStore& store);
QVector<AudioSource> classifyAudioSources(const PipeWireObjectStore& store);

} // namespace auralis::audio

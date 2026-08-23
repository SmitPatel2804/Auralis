#include <auralis/audio/AudioSourceListModel.h>

namespace auralis::audio {

AudioSourceListModel::AudioSourceListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void AudioSourceListModel::setSources(QVector<AudioSource> sources)
{
    if (sources_ == sources) {
        return;
    }
    beginResetModel();
    sources_ = std::move(sources);
    endResetModel();
}

QVector<AudioSource> AudioSourceListModel::sources() const
{
    return sources_;
}

int AudioSourceListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(sources_.size());
}

QVariant AudioSourceListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= sources_.size()) {
        return {};
    }
    const AudioSource& source = sources_.at(index.row());
    switch (role) {
    case SourceIdRole:
        return source.id;
    case NameRole:
    case Qt::DisplayRole: {
        // Prefer app streams as "Brave — …"; keep hardware as description (e.g. Built-in Analog).
        if (!source.applicationName.isEmpty()) {
            if (!source.description.isEmpty() && source.description != source.applicationName
                && source.description != source.nodeName) {
                return source.applicationName + QStringLiteral(" — ") + source.description;
            }
            return source.applicationName;
        }
        if (source.monitorSource) {
            const QString base = !source.description.isEmpty() ? source.description : source.nodeName;
            return base + QStringLiteral(" (monitor)");
        }
        if (source.sourceType == AudioSourceType::PhysicalAudioSource
            || source.sourceType == AudioSourceType::VirtualAudioSource) {
            const QString base = !source.description.isEmpty() ? source.description : source.nodeName;
            return base + QStringLiteral(" (mic)");
        }
        return !source.description.isEmpty() ? source.description : source.nodeName;
    }
    case DescriptionRole:
        return source.description;
    case MediaClassRole:
        return source.mediaClass;
    case SourceTypeRole:
        return toString(source.sourceType);
    case ApplicationNameRole:
        return source.applicationName;
    case AvailableRole:
        return source.available;
    case MonitorRole:
        return source.monitorSource;
    case PipeWireNodeIdRole:
        return source.pipeWireNodeId;
    default:
        return {};
    }
}

QHash<int, QByteArray> AudioSourceListModel::roleNames() const
{
    return {
        {SourceIdRole, "sourceId"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {MediaClassRole, "mediaClass"},
        {SourceTypeRole, "sourceType"},
        {ApplicationNameRole, "applicationName"},
        {AvailableRole, "available"},
        {MonitorRole, "monitorSource"},
        {PipeWireNodeIdRole, "pipeWireNodeId"},
    };
}

} // namespace auralis::audio

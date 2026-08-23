#include <auralis/audio/AudioSourceListModel.h>

namespace auralis::audio {

AudioSourceListModel::AudioSourceListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void AudioSourceListModel::setSources(QVector<AudioSource> sources)
{
    QVector<AudioSource> visible;
    visible.reserve(sources.size());
    for (const AudioSource& source : sources) {
        if (isUserSelectableAudioSource(source)) {
            visible.push_back(source);
        }
    }
    if (sources_ == visible) {
        return;
    }
    beginResetModel();
    sources_ = std::move(visible);
    endResetModel();
}

QVector<AudioSource> AudioSourceListModel::sources() const
{
    return sources_;
}

QString AudioSourceListModel::sourceIdAt(int row) const
{
    if (row < 0 || row >= sources_.size()) {
        return {};
    }
    return sources_.at(row).id;
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
    case Qt::DisplayRole:
        return sourceListDisplayName(source);
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

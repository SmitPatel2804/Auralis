#include <auralis/audio/AudioEndpointListModel.h>

namespace auralis::audio {

AudioEndpointListModel::AudioEndpointListModel(AudioEndpointRegistry* registry, QObject* parent)
    : QAbstractListModel(parent)
    , registry_(registry)
{
    if (registry_ == nullptr) {
        return;
    }
    connect(registry_, &AudioEndpointRegistry::endpointAboutToBeAdded, this, &AudioEndpointListModel::onAboutToBeAdded);
    connect(registry_, &AudioEndpointRegistry::endpointAdded, this, &AudioEndpointListModel::onAdded);
    connect(registry_, &AudioEndpointRegistry::endpointUpdated, this, &AudioEndpointListModel::onUpdated, Qt::QueuedConnection);
    connect(
        registry_,
        &AudioEndpointRegistry::endpointAboutToBeRemoved,
        this,
        &AudioEndpointListModel::onAboutToBeRemoved);
    connect(registry_, &AudioEndpointRegistry::endpointRemoved, this, &AudioEndpointListModel::onRemoved);
}

int AudioEndpointListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || registry_ == nullptr) {
        return 0;
    }
    return registry_->count();
}

QVariant AudioEndpointListModel::data(const QModelIndex& index, int role) const
{
    if (registry_ == nullptr || !index.isValid() || index.row() < 0 || index.row() >= registry_->count()) {
        return {};
    }
    const AudioEndpoint endpoint = registry_->at(index.row());
    switch (role) {
    case EndpointIdRole:
        return endpoint.id;
    case NameRole:
    case Qt::DisplayRole:
        return endpoint.name;
    case DescriptionRole:
        return endpoint.description;
    case DirectionRole:
        return toString(endpoint.direction);
    case AvailableRole:
        return endpoint.availability == AudioEndpointAvailability::Available;
    case TransportRole:
        return toString(endpoint.transport);
    case ProfileRole:
        return endpoint.profile;
    case CodecRole:
        return endpoint.codec;
    case PipeWireObjectIdRole:
        return endpoint.pipeWireObjectId;
    case BluetoothDeviceIdRole:
        return endpoint.bluetoothDeviceId;
    case BluetoothAddressRole:
        return endpoint.bluetoothAddress;
    case BluetoothDisplayNameRole:
        return endpoint.bluetoothDisplayName;
    case MappedRole:
        return endpoint.mapped();
    case MediaClassRole:
        return endpoint.mediaClass;
    case SampleRateRole:
        return endpoint.sampleRate.has_value() ? QVariant(*endpoint.sampleRate) : QVariant();
    case ChannelCountRole:
        return endpoint.channelCount.has_value() ? QVariant(*endpoint.channelCount) : QVariant();
    default:
        return {};
    }
}

QHash<int, QByteArray> AudioEndpointListModel::roleNames() const
{
    return {
        {EndpointIdRole, "endpointId"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {DirectionRole, "direction"},
        {AvailableRole, "available"},
        {TransportRole, "transport"},
        {ProfileRole, "profile"},
        {CodecRole, "codec"},
        {PipeWireObjectIdRole, "pipeWireObjectId"},
        {BluetoothDeviceIdRole, "bluetoothDeviceId"},
        {BluetoothAddressRole, "bluetoothAddress"},
        {BluetoothDisplayNameRole, "bluetoothDisplayName"},
        {MappedRole, "mapped"},
        {MediaClassRole, "mediaClass"},
        {SampleRateRole, "sampleRate"},
        {ChannelCountRole, "channelCount"},
    };
}

void AudioEndpointListModel::onAboutToBeAdded(int index)
{
    beginInsertRows(QModelIndex(), index, index);
}

void AudioEndpointListModel::onAdded(int)
{
    endInsertRows();
}

void AudioEndpointListModel::onUpdated(int index)
{
    if (index < 0 || registry_ == nullptr || index >= registry_->count()) {
        return;
    }
    const QModelIndex modelIndex = this->index(index);
    emit dataChanged(modelIndex, modelIndex);
}

void AudioEndpointListModel::onAboutToBeRemoved(int index, const QString&)
{
    beginRemoveRows(QModelIndex(), index, index);
}

void AudioEndpointListModel::onRemoved(int, const QString&)
{
    endRemoveRows();
}

} // namespace auralis::audio

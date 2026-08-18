#pragma once

#include <auralis/audio/AudioEndpointRegistry.h>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>

namespace auralis::audio {

class AudioEndpointListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        EndpointIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        DirectionRole,
        AvailableRole,
        TransportRole,
        ProfileRole,
        CodecRole,
        PipeWireObjectIdRole,
        BluetoothDeviceIdRole,
        BluetoothAddressRole,
        BluetoothDisplayNameRole,
        MappedRole,
        MediaClassRole,
        SampleRateRole,
        ChannelCountRole
    };

    explicit AudioEndpointListModel(AudioEndpointRegistry* registry, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    void onAboutToBeAdded(int index);
    void onAdded(int index);
    void onUpdated(int index);
    void onAboutToBeRemoved(int index, const QString& endpointId);
    void onRemoved(int index, const QString& endpointId);

    AudioEndpointRegistry* registry_ = nullptr;
};

} // namespace auralis::audio

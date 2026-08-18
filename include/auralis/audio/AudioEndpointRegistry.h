#pragma once

#include <auralis/audio/AudioEndpoint.h>

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace auralis::audio {

class AudioEndpointRegistry final : public QObject {
    Q_OBJECT

public:
    explicit AudioEndpointRegistry(QObject* parent = nullptr);

    bool upsert(AudioEndpoint endpoint);
    bool removeById(const QString& endpointId);
    bool removeByPipeWireObjectId(quint32 globalId);
    void clear();

    int count() const noexcept;
    int indexOf(const QString& endpointId) const;
    const AudioEndpoint* findById(const QString& endpointId) const;
    const AudioEndpoint* findByPipeWireObjectId(quint32 globalId) const;
    AudioEndpoint at(int index) const;
    QVector<AudioEndpoint> endpoints() const;
    QVector<AudioEndpoint> playbackEndpoints() const;
    QVector<AudioEndpoint> captureEndpoints() const;
    QVector<AudioEndpoint> endpointsForBluetoothDevice(const QString& bluetoothDeviceId) const;
    QVector<AudioEndpoint> mappedBluetoothEndpoints() const;
    QVector<AudioEndpoint> unmappedBluetoothEndpoints() const;

signals:
    void endpointAboutToBeAdded(int index);
    void endpointAdded(int index);
    void endpointUpdated(int index);
    void endpointAboutToBeRemoved(int index, const QString& endpointId);
    void endpointRemoved(int index, const QString& endpointId);
    void countChanged();

private:
    QStringList order_;
    QHash<QString, AudioEndpoint> byId_;
    QHash<quint32, QString> byPipeWireId_;
};

} // namespace auralis::audio

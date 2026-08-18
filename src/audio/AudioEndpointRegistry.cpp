#include <auralis/audio/AudioEndpointRegistry.h>

namespace auralis::audio {

AudioEndpointRegistry::AudioEndpointRegistry(QObject* parent)
    : QObject(parent)
{
}

bool AudioEndpointRegistry::upsert(AudioEndpoint endpoint)
{
    if (endpoint.id.isEmpty()) {
        return false;
    }

    const QString existingForPw = byPipeWireId_.value(endpoint.pipeWireObjectId);
    if (!existingForPw.isEmpty() && existingForPw != endpoint.id) {
        removeById(existingForPw);
    }

    const int existing = indexOf(endpoint.id);
    if (existing >= 0) {
        const AudioEndpoint previous = byId_.value(endpoint.id);
        if (previous.pipeWireObjectId != endpoint.pipeWireObjectId) {
            byPipeWireId_.remove(previous.pipeWireObjectId);
        }
        if (endpointPublicStateEqual(previous, endpoint)) {
            return false;
        }
        byId_.insert(endpoint.id, endpoint);
        byPipeWireId_.insert(endpoint.pipeWireObjectId, endpoint.id);
        emit endpointUpdated(existing);
        return false;
    }

    const int index = static_cast<int>(order_.size());
    emit endpointAboutToBeAdded(index);
    order_.push_back(endpoint.id);
    byId_.insert(endpoint.id, endpoint);
    byPipeWireId_.insert(endpoint.pipeWireObjectId, endpoint.id);
    emit endpointAdded(index);
    emit countChanged();
    return true;
}

bool AudioEndpointRegistry::removeById(const QString& endpointId)
{
    const int index = indexOf(endpointId);
    if (index < 0) {
        return false;
    }
    const AudioEndpoint removed = byId_.take(endpointId);
    byPipeWireId_.remove(removed.pipeWireObjectId);
    emit endpointAboutToBeRemoved(index, endpointId);
    order_.removeAt(index);
    emit endpointRemoved(index, endpointId);
    emit countChanged();
    return true;
}

bool AudioEndpointRegistry::removeByPipeWireObjectId(quint32 globalId)
{
    const QString endpointId = byPipeWireId_.value(globalId);
    if (endpointId.isEmpty()) {
        return false;
    }
    return removeById(endpointId);
}

void AudioEndpointRegistry::clear()
{
    if (order_.isEmpty()) {
        return;
    }
    const QStringList ids = order_;
    for (int i = static_cast<int>(ids.size()) - 1; i >= 0; --i) {
        removeById(ids.at(i));
    }
}

int AudioEndpointRegistry::count() const noexcept
{
    return static_cast<int>(order_.size());
}

int AudioEndpointRegistry::indexOf(const QString& endpointId) const
{
    return static_cast<int>(order_.indexOf(endpointId));
}

const AudioEndpoint* AudioEndpointRegistry::findById(const QString& endpointId) const
{
    const auto it = byId_.constFind(endpointId);
    if (it == byId_.cend()) {
        return nullptr;
    }
    return &it.value();
}

const AudioEndpoint* AudioEndpointRegistry::findByPipeWireObjectId(quint32 globalId) const
{
    const QString endpointId = byPipeWireId_.value(globalId);
    if (endpointId.isEmpty()) {
        return nullptr;
    }
    return findById(endpointId);
}

AudioEndpoint AudioEndpointRegistry::at(int index) const
{
    if (index < 0 || index >= order_.size()) {
        return {};
    }
    return byId_.value(order_.at(index));
}

QVector<AudioEndpoint> AudioEndpointRegistry::endpoints() const
{
    QVector<AudioEndpoint> result;
    result.reserve(order_.size());
    for (const QString& id : order_) {
        result.push_back(byId_.value(id));
    }
    return result;
}

QVector<AudioEndpoint> AudioEndpointRegistry::playbackEndpoints() const
{
    QVector<AudioEndpoint> result;
    for (const AudioEndpoint& endpoint : endpoints()) {
        if (endpoint.direction == AudioEndpointDirection::Playback || endpoint.direction == AudioEndpointDirection::Duplex) {
            result.push_back(endpoint);
        }
    }
    return result;
}

QVector<AudioEndpoint> AudioEndpointRegistry::captureEndpoints() const
{
    QVector<AudioEndpoint> result;
    for (const AudioEndpoint& endpoint : endpoints()) {
        if (endpoint.direction == AudioEndpointDirection::Capture || endpoint.direction == AudioEndpointDirection::Duplex) {
            result.push_back(endpoint);
        }
    }
    return result;
}

QVector<AudioEndpoint> AudioEndpointRegistry::endpointsForBluetoothDevice(const QString& bluetoothDeviceId) const
{
    QVector<AudioEndpoint> result;
    if (bluetoothDeviceId.isEmpty()) {
        return result;
    }
    for (const AudioEndpoint& endpoint : endpoints()) {
        if (endpoint.bluetoothDeviceId == bluetoothDeviceId) {
            result.push_back(endpoint);
        }
    }
    return result;
}

QVector<AudioEndpoint> AudioEndpointRegistry::mappedBluetoothEndpoints() const
{
    QVector<AudioEndpoint> result;
    for (const AudioEndpoint& endpoint : endpoints()) {
        if (endpoint.mapped()
            && (endpoint.transport == AudioTransport::BluetoothClassic
                || endpoint.transport == AudioTransport::BluetoothLE)) {
            result.push_back(endpoint);
        }
    }
    return result;
}

QVector<AudioEndpoint> AudioEndpointRegistry::unmappedBluetoothEndpoints() const
{
    QVector<AudioEndpoint> result;
    for (const AudioEndpoint& endpoint : endpoints()) {
        if (!endpoint.mapped()
            && (endpoint.transport == AudioTransport::BluetoothClassic
                || endpoint.transport == AudioTransport::BluetoothLE
                || !endpoint.bluetoothAddress.isEmpty() || !endpoint.bluezObjectPath.isEmpty())) {
            result.push_back(endpoint);
        }
    }
    return result;
}

} // namespace auralis::audio

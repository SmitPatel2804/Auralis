#include <auralis/audio/EndpointResolver.h>

#include <auralis/audio/AudioEndpointClassifier.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/bluetooth/BlueZTypes.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/LoggingCategories.h>

#include <QSet>

namespace auralis::audio {
namespace {

QString normalizePath(const QString& path)
{
    return path.trimmed();
}

QVector<auralis::bluetooth::BluetoothDeviceData> devicesMatchingAddress(
    const auralis::bluetooth::DeviceRegistry* registry,
    const QString& address)
{
    QVector<auralis::bluetooth::BluetoothDeviceData> matches;
    if (registry == nullptr || address.isEmpty()) {
        return matches;
    }
    const auto normalized = auralis::bluetooth::normalizeBluetoothAddress(address);
    if (!normalized.has_value()) {
        return matches;
    }
    for (const auto& device : registry->devices()) {
        const auto deviceAddress = auralis::bluetooth::normalizeBluetoothAddress(device.address);
        if (deviceAddress.has_value() && *deviceAddress == *normalized) {
            matches.push_back(device);
        }
    }
    return matches;
}

QVector<auralis::bluetooth::BluetoothDeviceData> devicesMatchingPath(
    const auralis::bluetooth::DeviceRegistry* registry,
    const QString& path)
{
    QVector<auralis::bluetooth::BluetoothDeviceData> matches;
    if (registry == nullptr || path.isEmpty()) {
        return matches;
    }
    const QString expected = normalizePath(path);
    for (const auto& device : registry->devices()) {
        if (normalizePath(device.objectPath) == expected) {
            matches.push_back(device);
        }
    }
    return matches;
}

void applyDevice(AudioEndpoint& endpoint, const auralis::bluetooth::BluetoothDeviceData& device, EndpointMappingReason reason)
{
    endpoint.bluetoothDeviceId = device.objectPath;
    endpoint.bluetoothAddress = auralis::bluetooth::normalizeBluetoothAddress(device.address).value_or(device.address);
    endpoint.bluetoothDisplayName = device.displayName();
    endpoint.bluezObjectPath = device.objectPath;
    endpoint.mappingReason = reason;
    endpoint.mappingConfidence = reason == EndpointMappingReason::UniqueNameFallback
        ? EndpointMappingConfidence::Weak
        : (reason == EndpointMappingReason::PipeWireDeviceOwnership ? EndpointMappingConfidence::Strong
                                                                    : EndpointMappingConfidence::Exact);
}

void clearMapping(AudioEndpoint& endpoint, EndpointMappingReason reason)
{
    endpoint.bluetoothDeviceId.clear();
    endpoint.bluetoothDisplayName.clear();
    endpoint.mappingReason = reason;
    endpoint.mappingConfidence = reason == EndpointMappingReason::Ambiguous ? EndpointMappingConfidence::None
                                                                            : EndpointMappingConfidence::None;
}

} // namespace

EndpointResolver::EndpointResolver(const bluetooth::DeviceRegistry* bluetoothRegistry)
    : bluetoothRegistry_(bluetoothRegistry)
{
}

void EndpointResolver::setBluetoothRegistry(const bluetooth::DeviceRegistry* bluetoothRegistry)
{
    bluetoothRegistry_ = bluetoothRegistry;
}

void EndpointResolver::applyMapping(AudioEndpoint& endpoint, const PipeWireObjectStore& store) const
{
    if (bluetoothRegistry_ == nullptr) {
        if (endpoint.transport == AudioTransport::BluetoothClassic || endpoint.transport == AudioTransport::BluetoothLE
            || !endpoint.bluetoothAddress.isEmpty() || !endpoint.bluezObjectPath.isEmpty()) {
            clearMapping(endpoint, EndpointMappingReason::InsufficientData);
        }
        return;
    }

    const auto addressMatches = devicesMatchingAddress(bluetoothRegistry_, endpoint.bluetoothAddress);
    if (addressMatches.size() == 1) {
        applyDevice(endpoint, addressMatches.front(), EndpointMappingReason::ExactBluetoothAddress);
        return;
    }
    if (addressMatches.size() > 1) {
        qCWarning(auralisAudio) << "EndpointResolver AmbiguousMapping endpoint=" << endpoint.id
                                << "candidateCount=" << addressMatches.size() << "signals=ExactBluetoothAddress";
        clearMapping(endpoint, EndpointMappingReason::Ambiguous);
        return;
    }

    const auto pathMatches = devicesMatchingPath(bluetoothRegistry_, endpoint.bluezObjectPath);
    if (pathMatches.size() == 1) {
        applyDevice(endpoint, pathMatches.front(), EndpointMappingReason::ExactBlueZObjectPath);
        return;
    }
    if (pathMatches.size() > 1) {
        qCWarning(auralisAudio) << "EndpointResolver AmbiguousMapping endpoint=" << endpoint.id
                                << "candidateCount=" << pathMatches.size() << "signals=ExactBlueZObjectPath";
        clearMapping(endpoint, EndpointMappingReason::Ambiguous);
        return;
    }

    if (endpoint.pipeWireDeviceId.has_value()) {
        if (const PipeWireDeviceInfo* device = store.device(*endpoint.pipeWireDeviceId)) {
            const auto ownedAddress = devicesMatchingAddress(
                bluetoothRegistry_,
                device->bluezAddress.value_or(QString()));
            if (ownedAddress.size() == 1) {
                applyDevice(endpoint, ownedAddress.front(), EndpointMappingReason::PipeWireDeviceOwnership);
                return;
            }
            const auto ownedPath = devicesMatchingPath(bluetoothRegistry_, device->bluezPath.value_or(QString()));
            if (ownedPath.size() == 1) {
                applyDevice(endpoint, ownedPath.front(), EndpointMappingReason::PipeWireDeviceOwnership);
                return;
            }
        }
    }

    if (endpoint.transport != AudioTransport::BluetoothClassic && endpoint.transport != AudioTransport::BluetoothLE
        && endpoint.bluetoothAddress.isEmpty() && endpoint.bluezObjectPath.isEmpty()) {
        clearMapping(endpoint, EndpointMappingReason::None);
        return;
    }

    QVector<auralis::bluetooth::BluetoothDeviceData> nameMatches;
    const QString needle = endpoint.name.trimmed();
    if (!needle.isEmpty()) {
        for (const auto& device : bluetoothRegistry_->devices()) {
            if (device.displayName().compare(needle, Qt::CaseInsensitive) == 0
                || device.alias.compare(needle, Qt::CaseInsensitive) == 0
                || device.name.compare(needle, Qt::CaseInsensitive) == 0) {
                nameMatches.push_back(device);
            }
        }
    }
    if (nameMatches.size() == 1) {
        applyDevice(endpoint, nameMatches.front(), EndpointMappingReason::UniqueNameFallback);
        return;
    }
    if (nameMatches.size() > 1) {
        qCWarning(auralisAudio) << "EndpointResolver AmbiguousMapping endpoint=" << endpoint.id
                                << "candidateCount=" << nameMatches.size() << "signals=UniqueNameFallback";
        clearMapping(endpoint, EndpointMappingReason::Ambiguous);
        return;
    }

    clearMapping(endpoint, EndpointMappingReason::InsufficientData);
}

void refreshEndpointsFromStore(
    const PipeWireObjectStore& store,
    AudioEndpointRegistry& registry,
    const EndpointResolver& resolver)
{
    QSet<quint32> liveIds;
    for (const PipeWireNodeInfo& node : store.nodes()) {
        std::optional<AudioEndpoint> candidate = classifyAudioEndpoint(node, store);
        if (!candidate.has_value()) {
            continue;
        }
        resolver.applyMapping(*candidate, store);
        candidate->id = makeEndpointLogicalId(*candidate, node);
        const bool inserted = registry.upsert(*candidate);
        if (inserted && candidate->mapped()) {
            qCInfo(auralisAudio) << "EndpointResolver Mapped endpoint=" << candidate->id
                                 << "device=" << candidate->bluetoothDeviceId
                                 << "reason=" << toString(candidate->mappingReason);
        }
        liveIds.insert(node.globalId);
    }

    const QVector<AudioEndpoint> current = registry.endpoints();
    for (const AudioEndpoint& endpoint : current) {
        if (!liveIds.contains(endpoint.pipeWireObjectId)) {
            qCInfo(auralisAudio) << "AudioEndpointRegistry EndpointRemoved endpoint=" << endpoint.id
                                 << "pw=" << endpoint.pipeWireObjectId;
            registry.removeById(endpoint.id);
        }
    }
}

} // namespace auralis::audio

#include <auralis/audio/PipeWireTypes.h>

#include <auralis/bluetooth/BlueZTypes.h>

#include <limits>

namespace auralis::audio {
namespace {

constexpr auto kKeyObjectSerial = "object.serial";
constexpr auto kKeyDeviceId = "device.id";
constexpr auto kKeyDeviceName = "device.name";
constexpr auto kKeyDeviceNick = "device.nick";
constexpr auto kKeyDeviceDescription = "device.description";
constexpr auto kKeyDeviceApi = "device.api";
constexpr auto kKeyDeviceBus = "device.bus";
constexpr auto kKeyNodeName = "node.name";
constexpr auto kKeyNodeNick = "node.nick";
constexpr auto kKeyNodeDescription = "node.description";
constexpr auto kKeyMediaClass = "media.class";
constexpr auto kKeyAudioChannels = "audio.channels";
constexpr auto kKeyAudioRate = "audio.rate";
constexpr auto kKeyBluezAddress = "api.bluez5.address";
constexpr auto kKeyBluezPath = "api.bluez5.path";
constexpr auto kKeyBluezDevice = "api.bluez5.device";
constexpr auto kKeyBluezProfile = "api.bluez5.profile";
constexpr auto kKeyBluezCodec = "api.bluez5.codec";

QString firstNonEmpty(const PipeWireProperties& properties, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = properties.value(key);
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

std::optional<QString> normalizedBluezAddress(const PipeWireProperties& properties)
{
    const QString raw = properties.value(QString::fromLatin1(kKeyBluezAddress));
    if (raw.isEmpty()) {
        return std::nullopt;
    }
    return auralis::bluetooth::normalizeBluetoothAddress(raw);
}

std::optional<QString> bluezPath(const PipeWireProperties& properties)
{
    const QString path = properties.value(QString::fromLatin1(kKeyBluezPath));
    if (!path.isEmpty()) {
        return path;
    }
    const QString device = properties.value(QString::fromLatin1(kKeyBluezDevice));
    if (!device.isEmpty()) {
        return device;
    }
    return std::nullopt;
}

} // namespace

QString toString(PipeWireConnectionState state)
{
    switch (state) {
    case PipeWireConnectionState::Stopped:
        return QStringLiteral("Stopped");
    case PipeWireConnectionState::Starting:
        return QStringLiteral("Starting");
    case PipeWireConnectionState::Connected:
        return QStringLiteral("Connected");
    case PipeWireConnectionState::Error:
        return QStringLiteral("Error");
    case PipeWireConnectionState::Stopping:
        return QStringLiteral("Stopping");
    }
    return QStringLiteral("Unknown");
}

PipeWireObjectKind kindFromInterfaceType(const QString& interfaceType)
{
    if (interfaceType == QLatin1String("PipeWire:Interface:Device")) {
        return PipeWireObjectKind::Device;
    }
    if (interfaceType == QLatin1String("PipeWire:Interface:Node")) {
        return PipeWireObjectKind::Node;
    }
    if (interfaceType == QLatin1String("PipeWire:Interface:Port")) {
        return PipeWireObjectKind::Port;
    }
    if (interfaceType == QLatin1String("PipeWire:Interface:Link")) {
        return PipeWireObjectKind::Link;
    }
    if (interfaceType == QLatin1String("PipeWire:Interface:Metadata")) {
        return PipeWireObjectKind::Metadata;
    }
    if (interfaceType.startsWith(QLatin1String("PipeWire:Interface:"))) {
        return PipeWireObjectKind::Other;
    }
    return PipeWireObjectKind::Unknown;
}

PipeWireDeviceInfo deviceInfoFromSnapshot(const PipeWireObjectSnapshot& snapshot)
{
    PipeWireDeviceInfo info;
    mergeDeviceInfo(info, snapshot);
    return info;
}

PipeWireNodeInfo nodeInfoFromSnapshot(const PipeWireObjectSnapshot& snapshot)
{
    PipeWireNodeInfo info;
    mergeNodeInfo(info, snapshot);
    return info;
}

void mergeDeviceInfo(PipeWireDeviceInfo& target, const PipeWireObjectSnapshot& snapshot)
{
    target.globalId = snapshot.globalId;
    target.properties.merge(snapshot.properties);
    if (const auto serial = target.properties.uintValue(QString::fromLatin1(kKeyObjectSerial))) {
        target.serial = serial;
    }
    const QString name = firstNonEmpty(
        target.properties,
        {QString::fromLatin1(kKeyDeviceName), QString::fromLatin1(kKeyNodeName)});
    if (!name.isEmpty()) {
        target.name = name;
    }
    const QString nick = target.properties.value(QString::fromLatin1(kKeyDeviceNick));
    if (!nick.isEmpty()) {
        target.nick = nick;
    }
    const QString description = firstNonEmpty(
        target.properties,
        {QString::fromLatin1(kKeyDeviceDescription), QString::fromLatin1(kKeyNodeDescription)});
    if (!description.isEmpty()) {
        target.description = description;
    }
    const QString api = target.properties.value(QString::fromLatin1(kKeyDeviceApi));
    if (!api.isEmpty()) {
        target.api = api;
    }
    const QString bus = target.properties.value(QString::fromLatin1(kKeyDeviceBus));
    if (!bus.isEmpty()) {
        target.bus = bus;
    }
    if (const auto address = normalizedBluezAddress(target.properties)) {
        target.bluezAddress = address;
    }
    if (const auto path = bluezPath(target.properties)) {
        target.bluezPath = path;
    }
    const QString profile = target.properties.value(QString::fromLatin1(kKeyBluezProfile));
    if (!profile.isEmpty()) {
        target.bluezProfile = profile;
    }
    const QString codec = target.properties.value(QString::fromLatin1(kKeyBluezCodec));
    if (!codec.isEmpty()) {
        target.bluezCodec = codec;
    }
}

void mergeNodeInfo(PipeWireNodeInfo& target, const PipeWireObjectSnapshot& snapshot)
{
    target.globalId = snapshot.globalId;
    target.properties.merge(snapshot.properties);
    if (const auto serial = target.properties.uintValue(QString::fromLatin1(kKeyObjectSerial))) {
        target.serial = serial;
    }
    if (const auto deviceId = target.properties.uintValue(QString::fromLatin1(kKeyDeviceId))) {
        if (deviceId.value() <= static_cast<quint64>(std::numeric_limits<quint32>::max())) {
            target.deviceId = static_cast<quint32>(deviceId.value());
        }
    }
    const QString name = firstNonEmpty(
        target.properties,
        {QString::fromLatin1(kKeyNodeName), QString::fromLatin1(kKeyDeviceName)});
    if (!name.isEmpty()) {
        target.name = name;
    }
    const QString nick = firstNonEmpty(
        target.properties,
        {QString::fromLatin1(kKeyNodeNick), QString::fromLatin1(kKeyDeviceNick)});
    if (!nick.isEmpty()) {
        target.nick = nick;
    }
    const QString description = firstNonEmpty(
        target.properties,
        {QString::fromLatin1(kKeyNodeDescription), QString::fromLatin1(kKeyDeviceDescription)});
    if (!description.isEmpty()) {
        target.description = description;
    }
    const QString mediaClass = target.properties.value(QString::fromLatin1(kKeyMediaClass));
    if (!mediaClass.isEmpty()) {
        target.mediaClass = mediaClass;
    }
    if (const auto address = normalizedBluezAddress(target.properties)) {
        target.bluezAddress = address;
    }
    if (const auto path = bluezPath(target.properties)) {
        target.bluezPath = path;
    }
    const QString profile = target.properties.value(QString::fromLatin1(kKeyBluezProfile));
    if (!profile.isEmpty()) {
        target.bluezProfile = profile;
    }
    const QString codec = target.properties.value(QString::fromLatin1(kKeyBluezCodec));
    if (!codec.isEmpty()) {
        target.bluezCodec = codec;
    }
    if (const auto channels = target.properties.uintValue(QString::fromLatin1(kKeyAudioChannels))) {
        if (channels.value() <= static_cast<quint64>(std::numeric_limits<quint32>::max())) {
            target.channelCount = static_cast<quint32>(channels.value());
        }
    }
    if (const auto rate = target.properties.uintValue(QString::fromLatin1(kKeyAudioRate))) {
        if (rate.value() <= static_cast<quint64>(std::numeric_limits<quint32>::max())) {
            target.sampleRate = static_cast<quint32>(rate.value());
        }
    }
}

} // namespace auralis::audio

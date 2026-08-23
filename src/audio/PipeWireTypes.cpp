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
constexpr auto kKeyApplicationName = "application.name";
constexpr auto kKeyApplicationProcessId = "application.process.id";
constexpr auto kKeyNodeId = "node.id";
constexpr auto kKeyPortName = "port.name";
constexpr auto kKeyPortAlias = "port.alias";
constexpr auto kKeyPortDirection = "port.direction";
constexpr auto kKeyPortMonitor = "port.monitor";
constexpr auto kKeyPortPhysical = "port.physical";
constexpr auto kKeyPortTerminal = "port.terminal";
constexpr auto kKeyPortControl = "port.control";
constexpr auto kKeyAudioChannel = "audio.channel";
constexpr auto kKeyLinkOutputNode = "link.output.node";
constexpr auto kKeyLinkOutputPort = "link.output.port";
constexpr auto kKeyLinkInputNode = "link.input.node";
constexpr auto kKeyLinkInputPort = "link.input.port";
constexpr auto kKeyLinkState = "link.state";
constexpr auto kKeyLinkError = "link.error";

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
    const QString application = target.properties.value(QString::fromLatin1(kKeyApplicationName));
    if (!application.isEmpty()) {
        target.applicationName = application;
    }
    if (const auto pid = target.properties.uintValue(QString::fromLatin1(kKeyApplicationProcessId))) {
        if (pid.value() <= static_cast<quint64>(std::numeric_limits<quint32>::max())) {
            target.processId = static_cast<quint32>(pid.value());
        }
    }
    if (const auto latencyNs = target.properties.uintValue(QStringLiteral("auralis.latency.input.ns"))) {
        target.inputLatencyNs = static_cast<qint64>(latencyNs.value());
    }
}

QString toString(PipeWirePortDirection direction)
{
    switch (direction) {
    case PipeWirePortDirection::Input:
        return QStringLiteral("Input");
    case PipeWirePortDirection::Output:
        return QStringLiteral("Output");
    case PipeWirePortDirection::Unknown:
        return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

QString toString(PipeWireLinkState state)
{
    switch (state) {
    case PipeWireLinkState::Error:
        return QStringLiteral("Error");
    case PipeWireLinkState::Unlinked:
        return QStringLiteral("Unlinked");
    case PipeWireLinkState::Init:
        return QStringLiteral("Init");
    case PipeWireLinkState::Negotiating:
        return QStringLiteral("Negotiating");
    case PipeWireLinkState::Allocating:
        return QStringLiteral("Allocating");
    case PipeWireLinkState::Paused:
        return QStringLiteral("Paused");
    case PipeWireLinkState::Active:
        return QStringLiteral("Active");
    case PipeWireLinkState::Unknown:
        return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

PipeWirePortDirection parsePortDirection(const QString& raw)
{
    const QString lowered = raw.trimmed().toLower();
    if (lowered == QLatin1String("in") || lowered == QLatin1String("input")) {
        return PipeWirePortDirection::Input;
    }
    if (lowered == QLatin1String("out") || lowered == QLatin1String("output")) {
        return PipeWirePortDirection::Output;
    }
    return PipeWirePortDirection::Unknown;
}

PipeWireLinkState parseLinkState(const QString& raw)
{
    const QString lowered = raw.trimmed().toLower();
    if (lowered == QLatin1String("error")) {
        return PipeWireLinkState::Error;
    }
    if (lowered == QLatin1String("unlinked")) {
        return PipeWireLinkState::Unlinked;
    }
    if (lowered == QLatin1String("init")) {
        return PipeWireLinkState::Init;
    }
    if (lowered == QLatin1String("negotiating")) {
        return PipeWireLinkState::Negotiating;
    }
    if (lowered == QLatin1String("allocating")) {
        return PipeWireLinkState::Allocating;
    }
    if (lowered == QLatin1String("paused") || lowered == QLatin1String("idle")) {
        return PipeWireLinkState::Paused;
    }
    if (lowered == QLatin1String("active")) {
        return PipeWireLinkState::Active;
    }
    return PipeWireLinkState::Unknown;
}

QString canonicalAudioChannel(const QString& raw)
{
    const QString lowered = raw.trimmed().toUpper();
    if (lowered.isEmpty() || lowered == QLatin1String("UNK") || lowered == QLatin1String("UNKNOWN")) {
        return {};
    }
    if (lowered == QLatin1String("FL") || lowered == QLatin1String("FRONT_LEFT") || lowered == QLatin1String("FRONT-LEFT")) {
        return QStringLiteral("FL");
    }
    if (lowered == QLatin1String("FR") || lowered == QLatin1String("FRONT_RIGHT") || lowered == QLatin1String("FRONT-RIGHT")) {
        return QStringLiteral("FR");
    }
    if (lowered == QLatin1String("FC") || lowered == QLatin1String("FRONT_CENTER") || lowered == QLatin1String("MONO")
        || lowered == QLatin1String("MONO1")) {
        return QStringLiteral("MONO");
    }
    if (lowered == QLatin1String("LFE") || lowered == QLatin1String("RL") || lowered == QLatin1String("RR")
        || lowered == QLatin1String("SL") || lowered == QLatin1String("SR") || lowered == QLatin1String("RC")) {
        return lowered;
    }
    return lowered;
}

PipeWirePortInfo portInfoFromSnapshot(const PipeWireObjectSnapshot& snapshot)
{
    PipeWirePortInfo info;
    mergePortInfo(info, snapshot);
    return info;
}

PipeWireLinkInfo linkInfoFromSnapshot(const PipeWireObjectSnapshot& snapshot)
{
    PipeWireLinkInfo info;
    mergeLinkInfo(info, snapshot);
    return info;
}

void mergePortInfo(PipeWirePortInfo& target, const PipeWireObjectSnapshot& snapshot)
{
    target.globalId = snapshot.globalId;
    target.properties.merge(snapshot.properties);
    if (const auto serial = target.properties.uintValue(QString::fromLatin1(kKeyObjectSerial))) {
        target.serial = serial;
    }
    if (const auto nodeId = target.properties.uintValue(QString::fromLatin1(kKeyNodeId))) {
        if (nodeId.value() <= static_cast<quint64>(std::numeric_limits<quint32>::max())) {
            target.nodeId = static_cast<quint32>(nodeId.value());
        }
    }
    const QString name = firstNonEmpty(
        target.properties,
        {QString::fromLatin1(kKeyPortName), QString::fromLatin1(kKeyNodeName)});
    if (!name.isEmpty()) {
        target.name = name;
    }
    const QString alias = target.properties.value(QString::fromLatin1(kKeyPortAlias));
    if (!alias.isEmpty()) {
        target.alias = alias;
    }
    const PipeWirePortDirection direction = parsePortDirection(target.properties.value(QString::fromLatin1(kKeyPortDirection)));
    if (direction != PipeWirePortDirection::Unknown) {
        target.direction = direction;
    }
    const QString channel = canonicalAudioChannel(target.properties.value(QString::fromLatin1(kKeyAudioChannel)));
    if (!channel.isEmpty()) {
        target.audioChannel = channel;
    }
    if (const auto monitor = target.properties.boolValue(QString::fromLatin1(kKeyPortMonitor))) {
        target.monitor = *monitor;
    }
    if (const auto physical = target.properties.boolValue(QString::fromLatin1(kKeyPortPhysical))) {
        target.physical = *physical;
    }
    if (const auto terminal = target.properties.boolValue(QString::fromLatin1(kKeyPortTerminal))) {
        target.terminal = *terminal;
    }
    if (const auto control = target.properties.boolValue(QString::fromLatin1(kKeyPortControl))) {
        target.control = *control;
    }
}

void mergeLinkInfo(PipeWireLinkInfo& target, const PipeWireObjectSnapshot& snapshot)
{
    target.globalId = snapshot.globalId;
    target.properties.merge(snapshot.properties);
    auto assignId = [&](const char* key, std::optional<quint32>& field) {
        if (const auto value = target.properties.uintValue(QString::fromLatin1(key))) {
            if (value.value() <= static_cast<quint64>(std::numeric_limits<quint32>::max())) {
                field = static_cast<quint32>(value.value());
            }
        }
    };
    assignId(kKeyLinkOutputNode, target.outputNode);
    assignId(kKeyLinkOutputPort, target.outputPort);
    assignId(kKeyLinkInputNode, target.inputNode);
    assignId(kKeyLinkInputPort, target.inputPort);
    const PipeWireLinkState state = parseLinkState(target.properties.value(QString::fromLatin1(kKeyLinkState)));
    if (state != PipeWireLinkState::Unknown) {
        target.state = state;
    }
    const QString error = target.properties.value(QString::fromLatin1(kKeyLinkError));
    if (!error.isEmpty()) {
        target.error = error;
    }
}

} // namespace auralis::audio

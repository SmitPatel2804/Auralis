#pragma once

#include <auralis/audio/AudioEndpoint.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/audio/PipeWireTypes.h>
#include <auralis/bluetooth/BlueZPropertyParser.h>
#include <auralis/bluetooth/BlueZTypes.h>

namespace auralis::test {

inline auralis::audio::PipeWireObjectSnapshot makeSnapshot(
    quint32 id,
    auralis::audio::PipeWireObjectKind kind,
    const QHash<QString, QString>& properties)
{
    auralis::audio::PipeWireObjectSnapshot snapshot;
    snapshot.globalId = id;
    snapshot.kind = kind;
    switch (kind) {
    case auralis::audio::PipeWireObjectKind::Device:
        snapshot.interfaceType = QStringLiteral("PipeWire:Interface:Device");
        break;
    case auralis::audio::PipeWireObjectKind::Node:
        snapshot.interfaceType = QStringLiteral("PipeWire:Interface:Node");
        break;
    case auralis::audio::PipeWireObjectKind::Port:
        snapshot.interfaceType = QStringLiteral("PipeWire:Interface:Port");
        break;
    case auralis::audio::PipeWireObjectKind::Link:
        snapshot.interfaceType = QStringLiteral("PipeWire:Interface:Link");
        break;
    default:
        snapshot.interfaceType = QStringLiteral("PipeWire:Interface:Other");
        break;
    }
    snapshot.properties = auralis::audio::PipeWireProperties::fromHash(properties);
    return snapshot;
}

inline auralis::audio::PipeWireObjectSnapshot makeDevice(
    quint32 id,
    const QHash<QString, QString>& properties)
{
    return makeSnapshot(id, auralis::audio::PipeWireObjectKind::Device, properties);
}

inline auralis::audio::PipeWireObjectSnapshot makeNode(
    quint32 id,
    const QHash<QString, QString>& properties)
{
    return makeSnapshot(id, auralis::audio::PipeWireObjectKind::Node, properties);
}

inline auralis::audio::PipeWireObjectSnapshot makePort(
    quint32 id,
    quint32 nodeId,
    const QString& direction,
    const QHash<QString, QString>& extra = {})
{
    QHash<QString, QString> properties = extra;
    properties.insert(QStringLiteral("node.id"), QString::number(nodeId));
    properties.insert(QStringLiteral("port.direction"), direction);
    if (!properties.contains(QStringLiteral("port.name"))) {
        properties.insert(QStringLiteral("port.name"), QStringLiteral("port-%1").arg(id));
    }
    return makeSnapshot(id, auralis::audio::PipeWireObjectKind::Port, properties);
}

inline auralis::audio::PipeWireObjectSnapshot makeLink(
    quint32 id,
    quint32 outputNode,
    quint32 outputPort,
    quint32 inputNode,
    quint32 inputPort,
    const QString& state = QStringLiteral("active"))
{
    return makeSnapshot(
        id,
        auralis::audio::PipeWireObjectKind::Link,
        {{QStringLiteral("link.output.node"), QString::number(outputNode)},
         {QStringLiteral("link.output.port"), QString::number(outputPort)},
         {QStringLiteral("link.input.node"), QString::number(inputNode)},
         {QStringLiteral("link.input.port"), QString::number(inputPort)},
         {QStringLiteral("link.state"), state}});
}

inline auralis::audio::AudioEndpoint makePlaybackEndpoint(
    const QString& id,
    quint32 pipeWireObjectId,
    const QString& name = QStringLiteral("Speakers"))
{
    auralis::audio::AudioEndpoint endpoint;
    endpoint.id = id;
    endpoint.pipeWireObjectId = pipeWireObjectId;
    endpoint.name = name;
    endpoint.direction = auralis::audio::AudioEndpointDirection::Playback;
    endpoint.availability = auralis::audio::AudioEndpointAvailability::Available;
    endpoint.mediaClass = QStringLiteral("Audio/Sink");
    return endpoint;
}

inline auralis::bluetooth::BluetoothDeviceData makeBluetoothDevice(
    const QString& path,
    const QString& address,
    const QString& name = QStringLiteral("Headphones"),
    bool connected = false)
{
    return auralis::bluetooth::parseDevice(
               path,
               {{QStringLiteral("Address"), address},
                {QStringLiteral("AddressType"), QStringLiteral("public")},
                {QStringLiteral("Name"), name},
                {QStringLiteral("Alias"), name},
                {QStringLiteral("Adapter"), QStringLiteral("/org/bluez/hci0")},
                {QStringLiteral("Paired"), true},
                {QStringLiteral("Connected"), connected},
                {QStringLiteral("Trusted"), true}})
        .device;
}

} // namespace auralis::test

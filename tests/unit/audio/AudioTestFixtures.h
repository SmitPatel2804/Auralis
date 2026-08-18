#pragma once

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

inline auralis::audio::PipeWireObjectSnapshot makeNode(
    quint32 id,
    const QHash<QString, QString>& properties)
{
    return makeSnapshot(id, auralis::audio::PipeWireObjectKind::Node, properties);
}

inline auralis::audio::PipeWireObjectSnapshot makeDevice(
    quint32 id,
    const QHash<QString, QString>& properties)
{
    return makeSnapshot(id, auralis::audio::PipeWireObjectKind::Device, properties);
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

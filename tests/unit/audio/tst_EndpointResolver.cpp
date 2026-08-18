#include "AudioTestFixtures.h"

#include <auralis/audio/AudioEndpointClassifier.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/EndpointResolver.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/bluetooth/DeviceRegistry.h>

#include <QSet>
#include <QSignalSpy>
#include <QtTest>

using auralis::audio::AudioEndpoint;
using auralis::audio::AudioEndpointRegistry;
using auralis::audio::EndpointMappingReason;
using auralis::audio::EndpointResolver;
using auralis::audio::PipeWireObjectStore;
using auralis::audio::refreshEndpointsFromStore;
using auralis::bluetooth::DeviceRegistry;
using auralis::test::makeBluetoothDevice;
using auralis::test::makeDevice;
using auralis::test::makeNode;

class TstEndpointResolver : public QObject {
    Q_OBJECT

private:
    static void apply(PipeWireObjectStore& store, DeviceRegistry& bluetooth, AudioEndpointRegistry& endpoints)
    {
        EndpointResolver resolver(&bluetooth);
        refreshEndpointsFromStore(store, endpoints, resolver);
    }

private slots:
    void exactAddress()
    {
        DeviceRegistry bluetooth;
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"),
            QStringLiteral("AA:BB:CC:DD:EE:FF"),
            QStringLiteral("Headphones")));
        PipeWireObjectStore store;
        store.upsert(makeNode(60, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("api.bluez5.address"), QStringLiteral("aa:bb:cc:dd:ee:ff")},
                                   {QStringLiteral("node.description"), QStringLiteral("Headphones")}}));
        AudioEndpointRegistry endpoints;
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 1);
        QCOMPARE(endpoints.at(0).bluetoothDeviceId, QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"));
        QVERIFY(endpoints.at(0).mappingReason == EndpointMappingReason::ExactBluetoothAddress);
    }

    void exactBlueZPath()
    {
        DeviceRegistry bluetooth;
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"),
            QStringLiteral("11:22:33:44:55:66")));
        PipeWireObjectStore store;
        store.upsert(makeNode(61, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("api.bluez5.path"), QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF")}}));
        AudioEndpointRegistry endpoints;
        apply(store, bluetooth, endpoints);
        QVERIFY(endpoints.at(0).mappingReason == EndpointMappingReason::ExactBlueZObjectPath);
        QCOMPARE(endpoints.at(0).bluetoothDeviceId, QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"));
    }

    void pipeWireDeviceOwnership()
    {
        DeviceRegistry bluetooth;
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"),
            QStringLiteral("AA:BB:CC:DD:EE:FF")));
        PipeWireObjectStore store;
        store.upsert(makeDevice(50, {{QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
                                     {QStringLiteral("device.api"), QStringLiteral("bluez5")}}));
        store.upsert(makeNode(60, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("device.id"), QStringLiteral("50")},
                                   {QStringLiteral("node.name"), QStringLiteral("bluez_output")}}));
        AudioEndpointRegistry endpoints;
        apply(store, bluetooth, endpoints);
        QVERIFY(endpoints.at(0).mappingReason == EndpointMappingReason::PipeWireDeviceOwnership);
        QCOMPARE(endpoints.at(0).bluetoothDeviceId, QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"));
    }

    void ambiguousNameDoesNotGuess()
    {
        DeviceRegistry bluetooth;
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA"),
            QStringLiteral("AA:AA:AA:AA:AA:AA"),
            QStringLiteral("Hearing Aid")));
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_BB"),
            QStringLiteral("BB:BB:BB:BB:BB:BB"),
            QStringLiteral("Hearing Aid")));
        PipeWireObjectStore store;
        store.upsert(makeNode(70, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("node.description"), QStringLiteral("Hearing Aid")},
                                   {QStringLiteral("device.api"), QStringLiteral("bluez5")}}));
        AudioEndpointRegistry endpoints;
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 1);
        QVERIFY(endpoints.at(0).bluetoothDeviceId.isEmpty());
        QVERIFY(endpoints.at(0).mappingReason == EndpointMappingReason::Ambiguous);
    }

    void bluetoothThenPipeWire()
    {
        DeviceRegistry bluetooth;
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"),
            QStringLiteral("AA:BB:CC:DD:EE:FF"),
            QStringLiteral("HP"),
            true));
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 0);
        store.upsert(makeDevice(50, {{QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")}}));
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 0);
        store.upsert(makeNode(60, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("device.id"), QStringLiteral("50")}}));
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 1);
        QVERIFY(endpoints.at(0).mapped());
    }

    void pipeWireThenBluetooth()
    {
        DeviceRegistry bluetooth;
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(60, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")}}));
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 1);
        QVERIFY(!endpoints.at(0).mapped());
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"),
            QStringLiteral("AA:BB:CC:DD:EE:FF")));
        apply(store, bluetooth, endpoints);
        QVERIFY(endpoints.at(0).mapped());
    }

    void nodeRemovedBeforeBluetoothDisconnect()
    {
        DeviceRegistry bluetooth;
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"),
            QStringLiteral("AA:BB:CC:DD:EE:FF")));
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(60, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")}}));
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 1);
        store.remove(60);
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 0);
    }

    void bluetoothDisconnectBeforeNodeRemoved()
    {
        DeviceRegistry bluetooth;
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"),
            QStringLiteral("AA:BB:CC:DD:EE:FF")));
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(60, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")}}));
        apply(store, bluetooth, endpoints);
        bluetooth.removeDevice(QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"));
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 1);
        QVERIFY(!endpoints.at(0).mapped());
        store.remove(60);
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 0);
    }

    void reconnectChangesGlobalId()
    {
        DeviceRegistry bluetooth;
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"),
            QStringLiteral("AA:BB:CC:DD:EE:FF")));
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(81, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
                                   {QStringLiteral("api.bluez5.profile"), QStringLiteral("a2dp-sink")}}));
        apply(store, bluetooth, endpoints);
        const QString logicalId = endpoints.at(0).id;
        store.remove(81);
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 0);
        store.upsert(makeNode(96, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
                                   {QStringLiteral("api.bluez5.profile"), QStringLiteral("a2dp-sink")}}));
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 1);
        QCOMPARE(endpoints.at(0).id, logicalId);
        QCOMPARE(endpoints.at(0).pipeWireObjectId, static_cast<quint32>(96));
        QVERIFY(endpoints.findByPipeWireObjectId(81) == nullptr);
    }

    void multipleEndpointsPerDevice()
    {
        DeviceRegistry bluetooth;
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"),
            QStringLiteral("AA:BB:CC:DD:EE:FF")));
        PipeWireObjectStore store;
        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                  {QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
                                  {QStringLiteral("api.bluez5.profile"), QStringLiteral("a2dp-sink")}}));
        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Source")},
                                  {QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
                                  {QStringLiteral("api.bluez5.profile"), QStringLiteral("headset-head-unit")}}));
        store.upsert(makeNode(3, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                  {QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
                                  {QStringLiteral("api.bluez5.profile"), QStringLiteral("headset-head-unit")}}));
        AudioEndpointRegistry endpoints;
        apply(store, bluetooth, endpoints);
        QCOMPARE(endpoints.count(), 3);
        QCOMPARE(endpoints.endpointsForBluetoothDevice(QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF")).size(), 3);
        QSet<QString> ids;
        for (const AudioEndpoint& endpoint : endpoints.endpoints()) {
            ids.insert(endpoint.id);
            QVERIFY(endpoint.mapped());
        }
        QCOMPARE(ids.size(), 3);
    }

    void mappingChangeEmitsUpdate()
    {
        DeviceRegistry bluetooth;
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(60, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")}}));
        apply(store, bluetooth, endpoints);
        QSignalSpy updated(&endpoints, &AudioEndpointRegistry::endpointUpdated);
        bluetooth.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"),
            QStringLiteral("AA:BB:CC:DD:EE:FF")));
        apply(store, bluetooth, endpoints);
        QVERIFY(updated.count() >= 1);
        QVERIFY(endpoints.at(0).mapped());
    }
};

QTEST_GUILESS_MAIN(TstEndpointResolver)
#include "tst_EndpointResolver.moc"

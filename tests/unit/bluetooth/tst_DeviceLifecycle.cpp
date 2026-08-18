#include <auralis/bluetooth/AdapterManager.h>
#include <auralis/bluetooth/BlueZAgent.h>
#include <auralis/bluetooth/BlueZPropertyParser.h>
#include <auralis/bluetooth/DeviceLifecycleManager.h>
#include <auralis/bluetooth/DeviceOperation.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/ReconnectPolicy.h>

#include "FakeBlueZClient.h"

#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::AdapterData;
using auralis::bluetooth::AdapterManager;
using auralis::bluetooth::BlueZAgent;
using auralis::bluetooth::BluetoothDeviceData;
using auralis::bluetooth::DeviceLifecycleManager;
using auralis::bluetooth::DeviceOperation;
using auralis::bluetooth::DeviceRegistry;
using auralis::bluetooth::ReconnectPolicy;
using auralis::bluetooth::parseDevice;
using auralis::test::FakeBlueZClient;

class TstDeviceLifecycle : public QObject {
    Q_OBJECT

private:
    static BluetoothDeviceData makeDevice(const QString& path, bool paired = false, bool connected = false, bool trusted = false)
    {
        return parseDevice(
                   path,
                   {{QStringLiteral("Address"), QStringLiteral("AA:BB:CC:DD:EE:01")},
                    {QStringLiteral("AddressType"), QStringLiteral("public")},
                    {QStringLiteral("Name"), QStringLiteral("Headphones")},
                    {QStringLiteral("Adapter"), QStringLiteral("/org/bluez/hci0")},
                    {QStringLiteral("Paired"), paired},
                    {QStringLiteral("Connected"), connected},
                    {QStringLiteral("Trusted"), trusted}})
            .device;
    }

    struct Harness {
        FakeBlueZClient client;
        DeviceRegistry registry;
        AdapterManager adapters;
        BlueZAgent agent{&client};
        ReconnectPolicy reconnect;
        DeviceLifecycleManager lifecycle{&client, &registry, &adapters, &agent, &reconnect};

        void seedAdapter()
        {
            AdapterData adapter;
            adapter.objectPath = QStringLiteral("/org/bluez/hci0");
            adapter.address = QStringLiteral("00:11:22:33:44:55");
            adapter.name = QStringLiteral("hci0");
            adapter.powered = true;
            adapter.available = true;
            adapters.upsertAdapter(adapter);
        }
    };

private slots:
    void trustWaitsForBlueZProperty()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, false, false));
        h.client.setAutoCompleteDeviceOps(true);

        h.lifecycle.trustDevice(path);
        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Trusting);

        h.registry.applyPropertyChanges(path, {{QStringLiteral("Trusted"), true}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Trusted"), true}}, {});
        device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Idle);
        QVERIFY(device->trusted);
    }

    void duplicateConnectIsRejected()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, false, true));
        h.client.setAutoCompleteDeviceOps(false);

        h.lifecycle.connectDevice(path);
        h.lifecycle.connectDevice(path);
        QCOMPARE(h.client.connectRequests(), 1);
    }

    void forgetBlockedWhilePairing()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, false, false, false));
        h.client.setAutoCompleteDeviceOps(false);

        h.lifecycle.pairDevice(path);
        h.lifecycle.forgetDevice(path);
        QCOMPARE(h.client.removeRequests(), 0);
    }

    void pairCompletesWhenPairedPropertyArrives()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path));
        h.client.setAutoCompleteDeviceOps(false);

        h.lifecycle.pairDevice(path);
        h.client.completePairSuccess(path);
        h.registry.applyPropertyChanges(path, {{QStringLiteral("Paired"), true}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Paired"), true}}, {});
        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Idle);
        QVERIFY(device->paired);
    }

    void inProgressConnectDoesNotFailImmediately()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, false, true));
        h.client.setAutoCompleteDeviceOps(false);

        h.lifecycle.connectDevice(path);
        emit h.client.connectDeviceFinished(
            path,
            false,
            QStringLiteral("org.bluez.Error.InProgress"),
            QStringLiteral("In progress"));
        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Connecting);
    }

    void userDisconnectSuppressesAutoReconnect()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        h.client.setAutoCompleteDeviceOps(true);

        h.lifecycle.disconnectDevice(path);
        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);
        QTest::qWait(300);
        QCOMPARE(dueSpy.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TstDeviceLifecycle)
#include "tst_DeviceLifecycle.moc"

#include "FakeBlueZClient.h"

#include <auralis/bluetooth/BluetoothManager.h>

#include <auralis/core/ServiceStatus.h>

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::BluetoothManager;
using auralis::test::FakeBlueZClient;

class TstBluetoothManager : public QObject {
    Q_OBJECT

private slots:
    void lifecycleWithoutHardware()
    {
        auto* client = new FakeBlueZClient;
        client->setBlueZAvailable(false);
        BluetoothManager manager(client);
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
        QVERIFY(manager.initialize());
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Ready);
        QVERIFY(!manager.available());
        QVERIFY(!manager.canStartScan());
        manager.shutdown();
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
        manager.shutdown();
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
    }

    void snapshotPopulatesAdapterAndDevices()
    {
        auto* client = new FakeBlueZClient;
        client->setAdapter(
            QStringLiteral("/org/bluez/hci0"),
            {{QStringLiteral("Address"), QStringLiteral("E8:9E:B4:13:4C:CC")},
             {QStringLiteral("Alias"), QStringLiteral("smit")},
             {QStringLiteral("Powered"), true}});
        client->setDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_01"),
            {{QStringLiteral("Address"), QStringLiteral("AA:BB:CC:DD:EE:01")},
             {QStringLiteral("Alias"), QStringLiteral("Hearing Aid L")},
             {QStringLiteral("AddressType"), QStringLiteral("random")}});

        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        QVERIFY(manager.available());
        QVERIFY(manager.adapterPowered());
        QCOMPARE(manager.adapterName(), QStringLiteral("smit"));
        QCOMPARE(manager.deviceCount(), 1);
        QVERIFY(manager.canStartScan());
        QVERIFY(manager.devices() != nullptr);
        QCOMPARE(manager.devices()->rowCount(), 1);
    }

    void startScanUsesClient()
    {
        auto* client = new FakeBlueZClient;
        client->setAdapter(
            QStringLiteral("/org/bluez/hci0"),
            {{QStringLiteral("Address"), QStringLiteral("E8:9E:B4:13:4C:CC")},
             {QStringLiteral("Powered"), true}});
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        manager.startScan();
        QCOMPARE(client->startRequests(), 1);
        QCOMPARE(client->lastStartPath(), QStringLiteral("/org/bluez/hci0"));
        QVERIFY(manager.scanning());
    }
};

QTEST_GUILESS_MAIN(TstBluetoothManager)
#include "tst_BluetoothManager.moc"

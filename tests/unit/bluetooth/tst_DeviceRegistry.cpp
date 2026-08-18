#include <auralis/bluetooth/DeviceRegistry.h>

#include <auralis/bluetooth/BlueZPropertyParser.h>

#include <QSet>
#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::BluetoothDeviceData;
using auralis::bluetooth::DeviceRegistry;
using auralis::bluetooth::parseDevice;

class TstDeviceRegistry : public QObject {
    Q_OBJECT

private:
    static BluetoothDeviceData makeDevice(
        const QString& path,
        const QString& address,
        const QString& name,
        const QString& addressType = QStringLiteral("public"))
    {
        return parseDevice(
                   path,
                   {{QStringLiteral("Address"), address},
                    {QStringLiteral("AddressType"), addressType},
                    {QStringLiteral("Name"), name},
                    {QStringLiteral("Adapter"), QStringLiteral("/org/bluez/hci0")}})
            .device;
    }

private slots:
    void addAndCount()
    {
        DeviceRegistry registry;
        QSignalSpy countSpy(&registry, &DeviceRegistry::countChanged);
        QVERIFY(registry.upsertDevice(makeDevice(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One"))));
        QCOMPARE(registry.count(), 1);
        QCOMPARE(countSpy.count(), 1);
    }

    void duplicatePathBecomesUpdate()
    {
        DeviceRegistry registry;
        QSignalSpy added(&registry, &DeviceRegistry::deviceAdded);
        auto device = makeDevice(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One"));
        QVERIFY(registry.upsertDevice(device));
        device.name = QStringLiteral("Updated");
        QVERIFY(!registry.upsertDevice(device));
        QCOMPARE(registry.count(), 1);
        QCOMPARE(added.count(), 1);
        QCOMPARE(registry.at(0).name, QStringLiteral("Updated"));
    }

    void updatePreservesOtherFields()
    {
        DeviceRegistry registry;
        auto device = makeDevice(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One"));
        device.hasRssi = true;
        device.rssi = -40;
        registry.upsertDevice(device);
        QVERIFY(registry.applyPropertyChanges(QStringLiteral("/a"), {{QStringLiteral("Name"), QStringLiteral("Two")}}, {}));
        QCOMPARE(registry.at(0).name, QStringLiteral("Two"));
        QVERIFY(registry.at(0).hasRssi);
        QCOMPARE(registry.at(0).rssi, static_cast<qint16>(-40));
    }

    void removeUnknownPathIsSafe()
    {
        DeviceRegistry registry;
        registry.removeDevice(QStringLiteral("/missing"));
        QCOMPARE(registry.count(), 0);
    }

    void removeDeviceEmits()
    {
        DeviceRegistry registry;
        QSignalSpy removed(&registry, &DeviceRegistry::deviceRemoved);
        registry.upsertDevice(makeDevice(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One")));
        registry.removeDevice(QStringLiteral("/a"));
        QCOMPARE(registry.count(), 0);
        QCOMPARE(removed.count(), 1);
    }

    void secondaryAddressLookup()
    {
        DeviceRegistry registry;
        registry.upsertDevice(makeDevice(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One")));
        QVERIFY(registry.findByAddress(
                    QStringLiteral("/org/bluez/hci0"),
                    QStringLiteral("AA:BB:CC:DD:EE:01"),
                    QStringLiteral("public"))
                != nullptr);
    }

    void sameNameDevicesRemainDistinct()
    {
        DeviceRegistry registry;
        registry.upsertDevice(makeDevice(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("Buds")));
        registry.upsertDevice(makeDevice(QStringLiteral("/b"), QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("Buds")));
        QCOMPARE(registry.count(), 2);
        QVERIFY(registry.findByObjectPath(QStringLiteral("/a")) != nullptr);
        QVERIFY(registry.findByObjectPath(QStringLiteral("/b")) != nullptr);
    }

    void lastSeenUpdatesOnChange()
    {
        DeviceRegistry registry;
        registry.upsertDevice(makeDevice(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One")));
        const QDateTime first = registry.at(0).lastSeen;
        QVERIFY(first.isValid());
        QTest::qWait(5);
        registry.applyPropertyChanges(QStringLiteral("/a"), {{QStringLiteral("RSSI"), -30}}, {});
        QVERIFY(registry.at(0).lastSeen >= first);
    }

    void reconcileRemovesMissing()
    {
        DeviceRegistry registry;
        registry.upsertDevice(makeDevice(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One")));
        registry.upsertDevice(makeDevice(QStringLiteral("/b"), QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("Two")));
        registry.reconcile(QSet<QString>{QStringLiteral("/a")});
        QCOMPARE(registry.count(), 1);
        QCOMPARE(registry.at(0).objectPath, QStringLiteral("/a"));
    }

    void malformedRssiEmitsWarningAndKeepsDevice()
    {
        DeviceRegistry registry;
        registry.upsertDevice(makeDevice(QStringLiteral("/a"), QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("One")));
        QSignalSpy warnings(&registry, &DeviceRegistry::parseWarning);
        QVERIFY(registry.applyPropertyChanges(
            QStringLiteral("/a"),
            {{QStringLiteral("RSSI"), QStringLiteral("not-a-number")}},
            {}));
        QCOMPARE(registry.count(), 1);
        QVERIFY(!registry.at(0).hasRssi);
        QCOMPARE(warnings.count(), 1);
        QCOMPARE(warnings.at(0).at(0).toString(), QStringLiteral("/a"));
        QCOMPARE(warnings.at(0).at(1).toString(), QStringLiteral("RSSI"));
    }
};

QTEST_GUILESS_MAIN(TstDeviceRegistry)
#include "tst_DeviceRegistry.moc"

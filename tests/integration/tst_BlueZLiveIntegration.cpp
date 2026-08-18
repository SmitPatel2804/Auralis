#include <auralis/bluetooth/BluetoothDeviceListModel.h>
#include <auralis/bluetooth/BluetoothManager.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QAbstractItemModel>
#include <QElapsedTimer>
#include <QSet>
#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::BluetoothDeviceListModel;
using auralis::bluetooth::BluetoothManager;

class TstBlueZLiveIntegration : public QObject {
    Q_OBJECT

private:
    static QSet<QString> objectPaths(QAbstractItemModel* model)
    {
        QSet<QString> paths;
        if (model == nullptr) {
            return paths;
        }
        for (int row = 0; row < model->rowCount(); ++row) {
            paths.insert(model->data(model->index(row, 0), BluetoothDeviceListModel::ObjectPathRole).toString());
        }
        return paths;
    }

    static bool modelContainsAddress(QAbstractItemModel* model, const QString& address)
    {
        if (model == nullptr || address.isEmpty()) {
            return false;
        }
        const QString expected = address.trimmed().toUpper();
        for (int row = 0; row < model->rowCount(); ++row) {
            const QString actual =
                model->data(model->index(row, 0), BluetoothDeviceListModel::AddressRole).toString().trimmed().toUpper();
            if (actual == expected) {
                return true;
            }
        }
        return false;
    }

    static bool sawLiveActivity(QSignalSpy& rowsInsertedSpy, QSignalSpy& dataChangedSpy, int timeoutMs)
    {
        const auto inserted = rowsInsertedSpy.count();
        const auto changed = dataChangedSpy.count();
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            if (rowsInsertedSpy.count() > inserted || dataChangedSpy.count() > changed) {
                return true;
            }
            QTest::qWait(50);
        }
        return rowsInsertedSpy.count() > inserted || dataChangedSpy.count() > changed;
    }

private slots:
    void liveBlueZRoundTrip()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_BLUETOOTH_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_BLUETOOTH_INTEGRATION=1 to run live BlueZ tests");
        }

        QDBusConnection bus = QDBusConnection::systemBus();
        QVERIFY2(bus.isConnected(), "AURALIS_RUN_BLUETOOTH_INTEGRATION=1 but the system bus is not connected");
        QVERIFY2(
            bus.interface() != nullptr && bus.interface()->isServiceRegistered(QStringLiteral("org.bluez")),
            "AURALIS_RUN_BLUETOOTH_INTEGRATION=1 but org.bluez is not available");

        const QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.bluez"),
            QStringLiteral("/"),
            QStringLiteral("org.freedesktop.DBus.ObjectManager"),
            QStringLiteral("GetManagedObjects"));
        const QDBusMessage reply = bus.call(message);
        QVERIFY2(
            reply.type() != QDBusMessage::ErrorMessage,
            qPrintable(QStringLiteral("GetManagedObjects failed: %1").arg(reply.errorMessage())));
        QVERIFY2(!reply.arguments().isEmpty(), "GetManagedObjects returned no arguments");

        BluetoothManager manager;
        QVERIFY(manager.initialize());
        QVERIFY2(manager.available(), "BlueZ is registered but Auralis did not mark it available");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.adapterAddress().isEmpty(), 5000);
        QVERIFY2(
            !manager.adapterAddress().isEmpty(),
            "AURALIS_RUN_BLUETOOTH_INTEGRATION=1 but no BlueZ Adapter1 was enumerated");
        QVERIFY2(
            manager.canStartScan(),
            qPrintable(QStringLiteral("AURALIS_RUN_BLUETOOTH_INTEGRATION=1 but scan cannot start: %1 (adapter=%2 %3 powered=%4)")
                           .arg(
                               manager.statusText(),
                               manager.adapterName(),
                               manager.adapterAddress(),
                               manager.adapterPowered() ? QStringLiteral("true") : QStringLiteral("false"))));

        QAbstractItemModel* model = manager.devices();
        QVERIFY(model != nullptr);
        QSignalSpy rowsInsertedSpy(model, &QAbstractItemModel::rowsInserted);
        QSignalSpy dataChangedSpy(model, &QAbstractItemModel::dataChanged);

        manager.startScan();
        QTRY_VERIFY_WITH_TIMEOUT(manager.scanning() || !manager.errorText().isEmpty(), 3000);
        if (!manager.scanning()) {
            QFAIL(qPrintable(QStringLiteral("StartDiscovery failed: %1").arg(manager.errorText())));
        }

        const bool sawActivity = sawLiveActivity(rowsInsertedSpy, dataChangedSpy, 15000);
        QVERIFY2(
            sawActivity,
            "No live Bluetooth device activity was observed. "
            "Put a nearby Classic/BLE device into discoverable/advertising mode "
            "and rerun with AURALIS_RUN_BLUETOOTH_INTEGRATION=1.");

        const QString expectedAddress = qEnvironmentVariable("AURALIS_EXPECT_DEVICE_ADDRESS").trimmed();
        if (!expectedAddress.isEmpty()) {
            QTRY_VERIFY_WITH_TIMEOUT(modelContainsAddress(model, expectedAddress), 15000);
            QVERIFY2(
                modelContainsAddress(model, expectedAddress),
                qPrintable(QStringLiteral("Expected device address %1 was not observed").arg(expectedAddress)));
        }

        const QSet<QString> firstCyclePaths = objectPaths(model);
        QCOMPARE(firstCyclePaths.size(), model->rowCount());

        manager.stopScan();
        QTRY_VERIFY_WITH_TIMEOUT(!manager.scanning(), 5000);
        QVERIFY(!manager.scanning());
        QVERIFY(manager.canStartScan());

        manager.startScan();
        QTRY_VERIFY_WITH_TIMEOUT(manager.scanning() || !manager.errorText().isEmpty(), 3000);
        QVERIFY2(manager.scanning(), qPrintable(QStringLiteral("Second StartDiscovery failed: %1").arg(manager.errorText())));
        QTest::qWait(500);

        QCOMPARE(objectPaths(model).size(), model->rowCount());

        manager.stopScan();
        QTRY_VERIFY_WITH_TIMEOUT(!manager.scanning(), 5000);
        QVERIFY(manager.canStartScan());
        manager.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstBlueZLiveIntegration)
#include "tst_BlueZLiveIntegration.moc"

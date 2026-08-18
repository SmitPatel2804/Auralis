#include <auralis/bluetooth/BluetoothManager.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QtTest>

using auralis::bluetooth::BluetoothManager;

class TstBlueZLiveIntegration : public QObject {
    Q_OBJECT

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

        if (!manager.canStartScan()) {
            qWarning() << "Live adapter cannot start scan:" << manager.statusText()
                       << "adapter=" << manager.adapterName() << manager.adapterAddress()
                       << "powered=" << manager.adapterPowered();
            return;
        }

        manager.startScan();
        QTRY_VERIFY_WITH_TIMEOUT(manager.scanning() || !manager.errorText().isEmpty(), 3000);
        if (!manager.scanning()) {
            QFAIL(qPrintable(QStringLiteral("StartDiscovery failed: %1").arg(manager.errorText())));
        }
        QTest::qWait(4000);
        qInfo() << "Live discovery adapter=" << manager.adapterName() << manager.adapterAddress()
                << "powered=" << manager.adapterPowered()
                << "adapterDiscovering=" << manager.adapterDiscovering()
                << "deviceCount=" << manager.deviceCount();
        manager.stopScan();
        QTRY_VERIFY_WITH_TIMEOUT(!manager.scanning() || manager.canStartScan(), 3000);
        manager.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstBlueZLiveIntegration)
#include "tst_BlueZLiveIntegration.moc"

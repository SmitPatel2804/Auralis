#include <auralis/bluetooth/BlueZTypes.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/NativeBluetoothManager.h>

#include <QDateTime>
#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QTest>

class TstNativeBluetoothManager final : public QObject {
    Q_OBJECT

private slots:
    void deviceDetailsExposeCompleteQmlContract()
    {
        auralis::bluetooth::NativeBluetoothManager manager;
        auto* registry = manager.deviceRegistry();
        QVERIFY(registry != nullptr);

        auralis::bluetooth::BluetoothDeviceData device;
        device.objectPath = QStringLiteral("native:test-device");
        device.address = QStringLiteral("00:11:22:33:44:55");
        device.addressType = QStringLiteral("public");
        device.name = QStringLiteral("Test Headphones");
        device.alias = QStringLiteral("Desk Headphones");
        device.rssi = -52;
        device.hasRssi = true;
        device.paired = true;
        device.connected = true;
        device.trusted = true;
        device.blocked = false;
        device.servicesResolved = true;
        device.classOfDevice = 0x240418u;
        device.hasClassOfDevice = true;
        device.appearance = 0x0941u;
        device.hasAppearance = true;
        device.icon = QStringLiteral("audio-headphones");
        device.uuids = {QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb")};
        device.lastSeen = QDateTime::currentDateTimeUtc();
        registry->upsertDevice(device);

        const QVariantMap details = manager.deviceDetails(device.objectPath);
        const QStringList requiredKeys{
            QStringLiteral("objectPath"),
            QStringLiteral("name"),
            QStringLiteral("alias"),
            QStringLiteral("displayName"),
            QStringLiteral("address"),
            QStringLiteral("addressType"),
            QStringLiteral("rssi"),
            QStringLiteral("hasRssi"),
            QStringLiteral("paired"),
            QStringLiteral("trusted"),
            QStringLiteral("connected"),
            QStringLiteral("blocked"),
            QStringLiteral("servicesResolved"),
            QStringLiteral("classOfDevice"),
            QStringLiteral("hasClass"),
            QStringLiteral("appearance"),
            QStringLiteral("hasAppearance"),
            QStringLiteral("lastSeen"),
            QStringLiteral("transport"),
            QStringLiteral("icon"),
            QStringLiteral("uuids"),
            QStringLiteral("buttonPolicy"),
            QStringLiteral("canControlButtons"),
            QStringLiteral("buttonEffectiveStatus"),
        };
        for (const QString& key : requiredKeys) {
            QVERIFY2(details.contains(key), qPrintable(QStringLiteral("Missing QML device-detail key: %1").arg(key)));
        }

        QCOMPARE(details.value(QStringLiteral("rssi")).toInt(), -52);
        QCOMPARE(details.value(QStringLiteral("hasRssi")).toBool(), true);
        QCOMPARE(details.value(QStringLiteral("classOfDevice")).toUInt(), 0x240418u);
        QCOMPARE(details.value(QStringLiteral("appearance")).toUInt(), 0x0941u);
        QVERIFY(!details.value(QStringLiteral("lastSeen")).toString().isEmpty());
        QCOMPARE(details.value(QStringLiteral("buttonPolicy")).toString(), QStringLiteral("UNAVAILABLE"));

        auralis::bluetooth::BluetoothDeviceData ble;
        ble.objectPath = QStringLiteral("native:test-ble");
        ble.address = QStringLiteral("10:20:30:40:50:60");
        ble.addressType = QStringLiteral("random");
        ble.name = QStringLiteral("Test Sensor");
        ble.supportsLowEnergy = true;
        registry->upsertDevice(ble);
        QCOMPARE(manager.classicDeviceCount(), 1);
        QCOMPARE(manager.lowEnergyDeviceCount(), 1);
        QCOMPARE(manager.classicDevices()->rowCount(), 1);
        QCOMPARE(manager.lowEnergyDevices()->rowCount(), 1);
    }

    void liveAdapterScanStartStop()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_NATIVE_BLUETOOTH_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_NATIVE_BLUETOOTH_INTEGRATION=1 to exercise the real Bluetooth adapter");
        }

        auralis::bluetooth::NativeBluetoothManager manager;
        QVERIFY(manager.initialize());
        if (!manager.available() || !manager.adapterPowered()) {
            QSKIP("The Windows Bluetooth adapter is unavailable or powered off");
        }

        QSignalSpy scanningSpy(&manager, &auralis::bluetooth::NativeBluetoothManager::scanningChanged);
        // Cached Windows paired/connected devices must be visible before any
        // discovery request, eliminating the old mandatory Refresh step.
        const int preloadedCount = manager.deviceCount();
        QVERIFY(preloadedCount >= manager.connectedDeviceCount());
        const QString expectedAddress =
            QString::fromLocal8Bit(qgetenv("AURALIS_EXPECT_DEVICE_ADDRESS")).trimmed().toUpper();
        const auto expectedDevice = [&manager, &expectedAddress]() {
            for (const auto& device : manager.deviceRegistry()->devices()) {
                if (expectedAddress.isEmpty() || device.address.trimmed().toUpper() == expectedAddress) {
                    return device.objectPath;
                }
            }
            return QString();
        };
        if (!expectedAddress.isEmpty() && qEnvironmentVariableIntValue("AURALIS_EXPECT_CONNECTED_BLUETOOTH") == 1) {
            QTRY_VERIFY_WITH_TIMEOUT(!expectedDevice().isEmpty(), 3000);
            const auto* preloaded = manager.deviceRegistry()->findByObjectPath(expectedDevice());
            QVERIFY(preloaded != nullptr);
            QVERIFY2(preloaded->connected, "OS-connected Bluetooth device was not preloaded before scanning");
        }

        manager.startScan();
        QTRY_VERIFY_WITH_TIMEOUT(manager.scanning(), 3000);
        QCOMPARE(manager.scanModeText(), QStringLiteral("Classic & audio"));
        QVERIFY(scanningSpy.count() >= 1);

        if (expectedAddress.isEmpty()) {
            QTest::qWait(2000);
        } else {
            QTRY_VERIFY_WITH_TIMEOUT(!expectedDevice().isEmpty(), 12000);
        }
        QTest::qWait(3000);

        manager.stopScan();
        QTRY_VERIFY_WITH_TIMEOUT(!manager.scanning(), 3000);
        QVERIFY2(manager.errorText().isEmpty(), qPrintable(manager.errorText()));
        QCOMPARE(manager.devices()->rowCount(), manager.deviceCount());

        for (const auto& device : manager.deviceRegistry()->devices()) {
            qInfo().noquote() << "native Bluetooth device"
                              << device.displayName()
                              << device.address
                              << "paired=" << device.paired
                              << "connected=" << device.connected
                              << "services=" << device.uuids.size();
        }

        if (!expectedAddress.isEmpty()) {
            const auto* device = manager.deviceRegistry()->findByObjectPath(expectedDevice());
            QVERIFY(device != nullptr);
            QCOMPARE(device->address.trimmed().toUpper(), expectedAddress);
            QVERIFY2(device->paired, "Expected Windows Bluetooth device is visible but not reported as paired");
            if (qEnvironmentVariableIntValue("AURALIS_EXPECT_CONNECTED_BLUETOOTH") == 1) {
                QVERIFY2(device->connected, "Expected Windows Bluetooth device is not reported as connected");
            }
            const QVariantMap details = manager.deviceDetails(device->objectPath);
            QCOMPARE(details.value(QStringLiteral("displayName")).toString(), device->displayName());
            QCOMPARE(details.value(QStringLiteral("connected")).toBool(), device->connected);

            if (device->connected) {
                const QString objectPath = device->objectPath;
                manager.disconnectDevice(objectPath);
                const auto* afterDisconnectRequest = manager.deviceRegistry()->findByObjectPath(objectPath);
                QVERIFY(afterDisconnectRequest != nullptr);
                QVERIFY(afterDisconnectRequest->connected);
                QVERIFY(afterDisconnectRequest->userDisconnectRequested);
                QVERIFY(!afterDisconnectRequest->lastErrorMessage.isEmpty());

                // Windows has no public Qt API for disconnecting an A2DP
                // profile, so this action intentionally directs the user to
                // OS settings. A subsequent reconnect/refresh must clear that
                // stale instruction when the profile is already connected.
                manager.reconnectDevice(objectPath);
                const auto* afterReconnect = manager.deviceRegistry()->findByObjectPath(objectPath);
                QVERIFY(afterReconnect != nullptr);
                QVERIFY(afterReconnect->connected);
                QVERIFY(!afterReconnect->userDisconnectRequested);
                QVERIFY(afterReconnect->lastErrorMessage.isEmpty());
            }
        }

        // Repeated start/stop must remain idempotent and must not duplicate rows.
        for (int cycle = 0; cycle < 3; ++cycle) {
            manager.startScan();
            QTRY_VERIFY_WITH_TIMEOUT(manager.scanning(), 3000);
            manager.startScan();
            QTest::qWait(100);
            manager.stopScan();
            QTRY_VERIFY_WITH_TIMEOUT(!manager.scanning(), 3000);
            manager.stopScan();
            QVERIFY2(manager.errorText().isEmpty(), qPrintable(manager.errorText()));
        }
        QCOMPARE(manager.devices()->rowCount(), manager.deviceCount());

        manager.startLowEnergyScan();
        QTRY_VERIFY_WITH_TIMEOUT(manager.scanning(), 3000);
        QCOMPARE(manager.scanModeText(), QStringLiteral("Bluetooth LE"));
        QTest::qWait(3000);
        manager.stopScan();
        QTRY_VERIFY_WITH_TIMEOUT(!manager.scanning(), 3000);
        QCOMPARE(manager.classicDevices()->rowCount(), manager.classicDeviceCount());
        QCOMPARE(manager.lowEnergyDevices()->rowCount(), manager.lowEnergyDeviceCount());
        qInfo() << "isolated transport counts classic=" << manager.classicDeviceCount()
                << "ble=" << manager.lowEnergyDeviceCount();
        manager.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstNativeBluetoothManager)
#include "tst_NativeBluetoothManager.moc"

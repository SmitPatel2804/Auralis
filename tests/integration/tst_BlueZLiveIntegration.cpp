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

#include <functional>

using auralis::bluetooth::BluetoothDeviceListModel;
using auralis::bluetooth::BluetoothManager;

class TstBlueZLiveIntegration : public QObject {
    Q_OBJECT

private:
    static QVariant roleValue(QAbstractItemModel* model, const QString& objectPath, int role)
    {
        if (model == nullptr || objectPath.isEmpty()) {
            return {};
        }
        for (int row = 0; row < model->rowCount(); ++row) {
            if (model->data(model->index(row, 0), BluetoothDeviceListModel::ObjectPathRole).toString() == objectPath) {
                return model->data(model->index(row, 0), role);
            }
        }
        return {};
    }

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

    static QStringList expectedAddresses()
    {
        QStringList list;
        const QString multi = qEnvironmentVariable("AURALIS_EXPECT_DEVICE_ADDRESSES").trimmed();
        if (!multi.isEmpty()) {
            const auto parts = multi.split(QLatin1Char(';'), Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                const QString normalized = part.trimmed().toUpper();
                if (!normalized.isEmpty() && !list.contains(normalized)) {
                    list.append(normalized);
                }
            }
        }
        const QString single = qEnvironmentVariable("AURALIS_EXPECT_DEVICE_ADDRESS").trimmed().toUpper();
        if (!single.isEmpty() && !list.contains(single)) {
            list.prepend(single);
        }
        return list;
    }

    static QString objectPathForAddress(QAbstractItemModel* model, const QString& address)
    {
        const QString expected = address.trimmed().toUpper();
        for (int row = 0; row < model->rowCount(); ++row) {
            const QString actual =
                model->data(model->index(row, 0), BluetoothDeviceListModel::AddressRole).toString().trimmed().toUpper();
            if (actual == expected) {
                return model->data(model->index(row, 0), BluetoothDeviceListModel::ObjectPathRole).toString();
            }
        }
        return {};
    }

    static QString deviceDiagnostic(QAbstractItemModel* model, const QString& objectPath)
    {
        return QStringLiteral("paired=%1 trusted=%2 connected=%3 operation=%4 error=%5")
            .arg(roleValue(model, objectPath, BluetoothDeviceListModel::PairedRole).toBool() ? "true" : "false",
                 roleValue(model, objectPath, BluetoothDeviceListModel::TrustedRole).toBool() ? "true" : "false",
                 roleValue(model, objectPath, BluetoothDeviceListModel::ConnectedRole).toBool() ? "true" : "false",
                 roleValue(model, objectPath, BluetoothDeviceListModel::OperationTextRole).toString(),
                 roleValue(model, objectPath, BluetoothDeviceListModel::LastErrorMessageRole).toString());
    }

    static void autoAcceptPairingPrompt(BluetoothManager& manager)
    {
        QObject* pending = manager.pendingPairingRequest();
        if (pending == nullptr || pending->property("needsInput").toBool()) {
            return;
        }
        const QString requestId = pending->property("requestId").toString();
        if (requestId.isEmpty()) {
            return;
        }
        qInfo("LiveTest auto-accepting pairing request %s (%s)",
              qPrintable(requestId),
              qPrintable(pending->property("message").toString()));
        manager.acceptPairingRequest(requestId);
    }

    static bool waitUntil(
        const std::function<bool()>& predicate,
        int timeoutMs,
        BluetoothManager* manager = nullptr)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            if (manager != nullptr) {
                autoAcceptPairingPrompt(*manager);
            }
            if (predicate()) {
                return true;
            }
            QTest::qWait(50);
        }
        if (manager != nullptr) {
            autoAcceptPairingPrompt(*manager);
        }
        return predicate();
    }

    static bool runLifecycleForDevice(BluetoothManager& manager, QAbstractItemModel* model, const QString& address)
    {
        QString objectPath;
        if (!waitUntil(
                [&]() {
                    objectPath = objectPathForAddress(model, address);
                    return !objectPath.isEmpty();
                },
                20000,
                &manager)) {
            qWarning("Expected device address %s was not observed", qPrintable(address));
            return false;
        }
        if (!manager.agentRegistered()) {
            qWarning("Agent was not registered before lifecycle operations");
            return false;
        }
        qInfo("LiveTest lifecycle start %s path=%s %s",
              qPrintable(address),
              qPrintable(objectPath),
              qPrintable(deviceDiagnostic(model, objectPath)));

        if (!roleValue(model, objectPath, BluetoothDeviceListModel::PairedRole).toBool()) {
            manager.pairDevice(objectPath);
            if (!waitUntil(
                    [&]() {
                        return roleValue(model, objectPath, BluetoothDeviceListModel::PairedRole).toBool();
                    },
                    120000,
                    &manager)) {
                qWarning("Pair failed for %s: %s", qPrintable(address), qPrintable(deviceDiagnostic(model, objectPath)));
                return false;
            }
        }

        if (!roleValue(model, objectPath, BluetoothDeviceListModel::TrustedRole).toBool()) {
            manager.trustDevice(objectPath);
            if (!waitUntil(
                    [&]() {
                        return roleValue(model, objectPath, BluetoothDeviceListModel::TrustedRole).toBool();
                    },
                    15000,
                    &manager)) {
                qWarning("Trust failed for %s: %s", qPrintable(address), qPrintable(deviceDiagnostic(model, objectPath)));
                return false;
            }
        }

        manager.connectDevice(objectPath);
        if (!waitUntil(
                [&]() {
                    return roleValue(model, objectPath, BluetoothDeviceListModel::ConnectedRole).toBool();
                },
                60000,
                &manager)) {
            qWarning("Connect failed for %s: %s", qPrintable(address), qPrintable(deviceDiagnostic(model, objectPath)));
            return false;
        }

        manager.disconnectDevice(objectPath);
        if (!waitUntil(
                [&]() {
                    return !roleValue(model, objectPath, BluetoothDeviceListModel::ConnectedRole).toBool();
                },
                15000,
                &manager)) {
            qWarning("Disconnect failed for %s: %s", qPrintable(address), qPrintable(deviceDiagnostic(model, objectPath)));
            return false;
        }

        manager.reconnectDevice(objectPath);
        if (!waitUntil(
                [&]() {
                    return roleValue(model, objectPath, BluetoothDeviceListModel::ConnectedRole).toBool();
                },
                60000,
                &manager)) {
            qWarning("Reconnect failed for %s: %s", qPrintable(address), qPrintable(deviceDiagnostic(model, objectPath)));
            return false;
        }

        manager.disconnectDevice(objectPath);
        if (!waitUntil(
                [&]() {
                    return !roleValue(model, objectPath, BluetoothDeviceListModel::ConnectedRole).toBool();
                },
                15000,
                &manager)) {
            qWarning("Final disconnect failed for %s: %s", qPrintable(address), qPrintable(deviceDiagnostic(model, objectPath)));
            return false;
        }

        if (qEnvironmentVariableIntValue("AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS") == 1) {
            manager.forgetDevice(objectPath);
            if (!waitUntil(
                    [&]() {
                        return !objectPaths(model).contains(objectPath);
                    },
                    15000,
                    &manager)) {
                qWarning("Forget failed for %s", qPrintable(address));
                return false;
            }
        } else {
            qInfo("SKIPPED: destructive Bluetooth operations not enabled for %s", qPrintable(address));
        }
        qInfo("LiveTest lifecycle pass %s", qPrintable(address));
        return true;
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
        QTRY_VERIFY_WITH_TIMEOUT(manager.agentRegistered(), 5000);
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

        const QStringList addresses = expectedAddresses();
        QStringList connectedTogether;
        for (const QString& address : addresses) {
            QVERIFY2(
                waitUntil([&]() { return modelContainsAddress(model, address); }, 20000, &manager),
                qPrintable(QStringLiteral("Expected device address %1 was not observed").arg(address)));
            QVERIFY2(
                runLifecycleForDevice(manager, model, address),
                qPrintable(QStringLiteral("Lifecycle failed for %1").arg(address)));
        }

        if (addresses.size() >= 2 && qEnvironmentVariableIntValue("AURALIS_ALLOW_DESTRUCTIVE_BLUETOOTH_TESTS") != 1) {
            QStringList paths;
            for (const QString& address : addresses) {
                const QString path = objectPathForAddress(model, address);
                QVERIFY2(!path.isEmpty(), qPrintable(QStringLiteral("Missing path for dual-connect %1").arg(address)));
                paths.append(path);
                manager.connectDevice(path);
                const bool connected = waitUntil(
                    [&]() {
                        return roleValue(model, path, BluetoothDeviceListModel::ConnectedRole).toBool();
                    },
                    60000,
                    &manager);
                if (!connected) {
                    QWARN(qPrintable(QStringLiteral("Dual-connect could not connect %1: %2")
                                         .arg(address, deviceDiagnostic(model, path))));
                    continue;
                }
                connectedTogether.append(address);
            }
            qInfo("LiveTest simultaneous connected count=%d/%d",
                  static_cast<int>(connectedTogether.size()),
                  static_cast<int>(addresses.size()));
            if (connectedTogether.size() < addresses.size()) {
                QWARN("Controller did not keep every audio device connected at once (common A2DP adapter limit)");
            }
            for (const QString& path : paths) {
                if (roleValue(model, path, BluetoothDeviceListModel::ConnectedRole).toBool()) {
                    manager.disconnectDevice(path);
                    waitUntil(
                        [&]() {
                            return !roleValue(model, path, BluetoothDeviceListModel::ConnectedRole).toBool();
                        },
                        15000,
                        &manager);
                }
            }
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

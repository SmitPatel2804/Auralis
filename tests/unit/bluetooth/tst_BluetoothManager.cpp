#include "FakeBlueZClient.h"

#include <auralis/bluetooth/BluetoothManager.h>

#include <auralis/bluetooth/ReconnectPolicy.h>
#include <auralis/core/ServiceStatus.h>

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::BluetoothManager;
using auralis::bluetooth::ReconnectPolicyConfig;
using auralis::test::FakeBlueZClient;

class TstBluetoothManager : public QObject {
    Q_OBJECT

private:
    static QVariantMap poweredAdapter(bool discovering = false)
    {
        return {
            {QStringLiteral("Address"), QStringLiteral("E8:9E:B4:13:4C:CC")},
            {QStringLiteral("Alias"), QStringLiteral("smit")},
            {QStringLiteral("Powered"), true},
            {QStringLiteral("Discovering"), discovering},
        };
    }

    static QVariantMap classicDevice()
    {
        return {
            {QStringLiteral("Address"), QStringLiteral("AA:BB:CC:DD:EE:01")},
            {QStringLiteral("Alias"), QStringLiteral("Hearing Aid L")},
            {QStringLiteral("AddressType"), QStringLiteral("public")},
            {QStringLiteral("Adapter"), QStringLiteral("/org/bluez/hci0")},
        };
    }

    static void addPoweredAdapter(FakeBlueZClient* client, bool discovering = false)
    {
        client->setAdapter(QStringLiteral("/org/bluez/hci0"), poweredAdapter(discovering));
    }

    static QVariantMap pairedConnectedDevice()
    {
        QVariantMap device = classicDevice();
        device.insert(QStringLiteral("Paired"), true);
        device.insert(QStringLiteral("Connected"), true);
        return device;
    }

    static void configureFastReconnect(BluetoothManager& manager)
    {
        ReconnectPolicyConfig config;
        config.initialDelayMs = 50;
        config.maxAttempts = 3;
        config.maxDelayMs = 100;
        config.backoffMultiplier = 1.0;
        manager.setReconnectPolicyConfig(config);
    }

    static const QString kDevicePath;
    static const QString kAdapterPath;

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
        addPoweredAdapter(client);
        client->setDevice(QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_01"), classicDevice());

        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        QVERIFY(manager.available());
        QVERIFY(manager.adapterPowered());
        QCOMPARE(manager.adapterName(), QStringLiteral("smit"));
        QCOMPARE(manager.deviceCount(), 1);
        QVERIFY(manager.canStartScan());
        QVERIFY(manager.devices() != nullptr);
        QCOMPARE(manager.devices()->rowCount(), 1);
        QVERIFY(manager.errorText().isEmpty());
        QCOMPARE(manager.statusText(), QStringLiteral("Ready to scan"));
    }

    void startScanUsesClient()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        manager.startScan();
        QCOMPARE(client->startRequests(), 1);
        QCOMPARE(client->lastStartPath(), QStringLiteral("/org/bluez/hci0"));
        QVERIFY(manager.scanning());
    }

    void noAdapterErrorClearsAfterArrival()
    {
        auto* client = new FakeBlueZClient;
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        QVERIFY(manager.available());
        QVERIFY(!manager.canStartScan());
        QCOMPARE(manager.errorText(), QStringLiteral("Bluetooth adapter not available"));

        addPoweredAdapter(client);
        QVERIFY(manager.canStartScan());
        QCOMPARE(manager.statusText(), QStringLiteral("Ready to scan"));
        QVERIFY(manager.errorText().isEmpty());
    }

    void adapterPoweredOffClearsAfterPowerOn()
    {
        auto* client = new FakeBlueZClient;
        client->setAdapter(
            QStringLiteral("/org/bluez/hci0"),
            {{QStringLiteral("Address"), QStringLiteral("E8:9E:B4:13:4C:CC")},
             {QStringLiteral("Alias"), QStringLiteral("smit")},
             {QStringLiteral("Powered"), false}});
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        QVERIFY(!manager.canStartScan());
        QCOMPARE(manager.errorText(), QStringLiteral("Bluetooth is powered off"));
        QCOMPARE(manager.statusText(), QStringLiteral("Bluetooth is powered off"));

        client->updateAdapter(QStringLiteral("/org/bluez/hci0"), {{QStringLiteral("Powered"), true}});
        QVERIFY(manager.adapterPowered());
        QVERIFY(manager.canStartScan());
        QVERIFY(manager.errorText().isEmpty());
        QCOMPARE(manager.statusText(), QStringLiteral("Ready to scan"));
    }

    void blueZUnavailableClearsAfterReturn()
    {
        auto* client = new FakeBlueZClient;
        client->setBlueZAvailable(false);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        QVERIFY(!manager.available());
        QCOMPARE(manager.errorText(), QStringLiteral("BlueZ is unavailable"));

        client->setBlueZAvailable(true);
        addPoweredAdapter(client);
        QVERIFY(manager.available());
        QVERIFY(manager.canStartScan());
        QVERIFY(manager.errorText().isEmpty());
        QCOMPARE(manager.statusText(), QStringLiteral("Ready to scan"));
    }

    void snapshotFailureClearsAfterSuccessfulRefresh()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setDevice(QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_01"), classicDevice());
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        QCOMPARE(manager.deviceCount(), 1);
        QVERIFY(manager.errorText().isEmpty());

        client->failSnapshot(QStringLiteral("org.freedesktop.DBus.Error.Failed"), QStringLiteral("GetManagedObjects failed"));
        QVERIFY(manager.errorText().contains(QStringLiteral("Failed to refresh BlueZ state")));
        QVERIFY(manager.errorText().contains(QStringLiteral("GetManagedObjects failed")));
        QCOMPARE(manager.deviceCount(), 1);

        manager.refresh();
        QVERIFY(manager.errorText().isEmpty());
        QCOMPARE(manager.deviceCount(), 1);
        QVERIFY(manager.canStartScan());
    }

    void noSystemBusIsDistinct()
    {
        auto* client = new FakeBlueZClient;
        client->setSystemBusConnected(false);
        client->setBlueZAvailable(false);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        QCOMPARE(manager.statusText(), QStringLiteral("System D-Bus is unavailable"));
        QCOMPARE(manager.errorText(), QStringLiteral("System D-Bus is unavailable"));
        QVERIFY(!manager.errorText().contains(QStringLiteral("BlueZ is unavailable")));
        QVERIFY(!manager.canStartScan());
    }

    void startPendingThenStopSerializes()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setAutoCompleteStart(false);
        client->setAutoCompleteStop(false);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());

        manager.startScan();
        QCOMPARE(client->startRequests(), 1);
        QCOMPARE(client->stopRequests(), 0);
        QVERIFY(manager.canStopScan());
        QVERIFY(!manager.canStartScan());

        manager.stopScan();
        QCOMPARE(client->startRequests(), 1);
        QCOMPARE(client->stopRequests(), 0);

        client->completeStartSuccess();
        QCOMPARE(client->stopRequests(), 1);

        client->completeStopSuccess();
        QVERIFY(!manager.scanning());
        QVERIFY(manager.canStartScan());
        QVERIFY(!manager.canStopScan());
        QCOMPARE(manager.statusText(), QStringLiteral("Ready to scan"));
    }

    void startStopStartWhileStartPendingKeepsScanning()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setAutoCompleteStart(false);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());

        manager.startScan();
        manager.stopScan();
        manager.startScan();
        client->completeStartSuccess();

        QCOMPARE(client->stopRequests(), 0);
        QVERIFY(manager.scanning());
        QVERIFY(manager.canStopScan());
    }

    void duplicateStartWhilePendingDoesNotRetrigger()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setAutoCompleteStart(false);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        manager.startScan();
        manager.startScan();
        QCOMPARE(client->startRequests(), 1);
        client->completeStartSuccess();
        QVERIFY(manager.scanning());
        QCOMPARE(client->startRequests(), 1);
    }

    void startFailureAfterStopDoesNotSendStop()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setAutoCompleteStart(false);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());

        manager.startScan();
        manager.stopScan();
        client->completeStartFailure(QStringLiteral("org.bluez.Error.Failed"), QStringLiteral("rejected"));

        QCOMPARE(client->stopRequests(), 0);
        QVERIFY(!manager.scanning());
        QVERIFY(manager.canStartScan());
        QVERIFY(manager.errorText().contains(QStringLiteral("rejected")));
    }

    void stopPendingThenStartWaitsForStop()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setAutoCompleteStop(false);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());

        manager.startScan();
        QCOMPARE(client->startRequests(), 1);
        QVERIFY(manager.scanning());

        manager.stopScan();
        QCOMPARE(client->stopRequests(), 1);
        manager.startScan();
        QCOMPARE(client->startRequests(), 1);

        client->completeStopSuccess();
        QCOMPARE(client->startRequests(), 2);
        QVERIFY(manager.scanning());
    }

    void stopFailureDoesNotClaimCleanStop()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setAutoCompleteStop(false);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        manager.startScan();
        QVERIFY(manager.scanning());

        manager.stopScan();
        client->completeStopFailure(QStringLiteral("org.bluez.Error.Failed"), QStringLiteral("busy"));
        QVERIFY(manager.scanning());
        QVERIFY(manager.errorText().contains(QStringLiteral("busy")));
        QVERIFY(manager.canStopScan());
        QVERIFY(manager.canStartScan());
    }

    void otherClientScanningDoesNotSetLocalOwnership()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client, true);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        QVERIFY(manager.adapterDiscovering());
        QVERIFY(!manager.scanning());
        QVERIFY(manager.canStartScan());
    }

    void adapterDiscoveringFalseReconcilesLocalOwnership()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        manager.startScan();
        QVERIFY(manager.scanning());

        client->updateAdapter(QStringLiteral("/org/bluez/hci0"), {{QStringLiteral("Discovering"), false}});
        QVERIFY(!manager.scanning());
        QVERIFY(manager.canStartScan());
        QVERIFY(!manager.errorText().isEmpty());
    }

    void snapshotDiscoveringFalseReconcilesLocalOwnership()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        QVERIFY(manager.canStartScan());

        manager.startScan();
        QVERIFY(manager.scanning());

        client->replaceAdapter(QStringLiteral("/org/bluez/hci0"), poweredAdapter(false));
        manager.refresh();
        QVERIFY(!manager.scanning());
        QVERIFY(!manager.adapterDiscovering());
        QVERIFY(manager.canStartScan());
    }

    void globalDiscoveringRemainsTrueAfterLocalStop()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client, true);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        manager.startScan();
        QVERIFY(manager.scanning());
        QVERIFY(manager.adapterDiscovering());

        manager.stopScan();
        QVERIFY(!manager.scanning());
        QVERIFY(manager.adapterDiscovering());
        QVERIFY(manager.canStartScan());
    }

    void lateCallbackAfterBlueZRestartIsIgnored()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setAutoCompleteStart(false);
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());

        manager.startScan();
        QCOMPARE(client->startRequests(), 1);
        client->setBlueZAvailable(false);
        QVERIFY(!manager.scanning());
        QCOMPARE(manager.errorText(), QStringLiteral("BlueZ is unavailable"));

        client->completeStartSuccess();
        QVERIFY(!manager.scanning());

        client->setBlueZAvailable(true);
        addPoweredAdapter(client);
        QVERIFY(manager.available());
        QVERIFY(!manager.scanning());
        QVERIFY(manager.canStartScan());
        QVERIFY(manager.errorText().isEmpty());
        QCOMPARE(manager.statusText(), QStringLiteral("Ready to scan"));
    }

    void snapshotThenSamePathDoesNotDuplicate()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setDevice(QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_01"), classicDevice());
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        QCOMPARE(manager.deviceCount(), 1);

        QAbstractItemModel* model = manager.devices();
        QSignalSpy inserted(model, &QAbstractItemModel::rowsInserted);
        QSignalSpy dataChanged(model, &QAbstractItemModel::dataChanged);
        client->setDevice(
            QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_01"),
            {{QStringLiteral("Address"), QStringLiteral("AA:BB:CC:DD:EE:01")},
             {QStringLiteral("Alias"), QStringLiteral("Hearing Aid L Updated")},
             {QStringLiteral("Adapter"), QStringLiteral("/org/bluez/hci0")}});
        QCOMPARE(manager.deviceCount(), 1);
        QCOMPARE(inserted.count(), 0);
        QTRY_VERIFY(dataChanged.count() >= 1);
        QCOMPARE(
            model->data(model->index(0, 0), Qt::DisplayRole).toString(),
            QStringLiteral("Hearing Aid L Updated"));
    }

    void successfulStartClearsPriorStartError()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setStartResult(false, QStringLiteral("org.bluez.Error.Failed"), QStringLiteral("first"));
        BluetoothManager manager(client);
        QVERIFY(manager.initialize());
        manager.startScan();
        QVERIFY(manager.errorText().contains(QStringLiteral("first")));
        QVERIFY(!manager.scanning());

        client->setStartResult(true);
        manager.startScan();
        QVERIFY(manager.scanning());
        QVERIFY(manager.errorText().isEmpty());
    }

    void explicitDisconnectSurvivesBlueZRestart()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setDevice(kDevicePath, pairedConnectedDevice());

        BluetoothManager manager(client);
        configureFastReconnect(manager);
        QVERIFY(manager.initialize());
        QCOMPARE(manager.deviceCount(), 1);

        manager.disconnectDevice(kDevicePath);
        client->updateDevice(kDevicePath, {{QStringLiteral("Connected"), false}}, {});
        QVERIFY(manager.userDisconnectRequestedForDevice(kDevicePath));

        client->setBlueZAvailable(false);
        QCOMPARE(manager.deviceCount(), 0);

        client->setBlueZAvailable(true);
        client->requestSnapshot();
        QCOMPARE(manager.deviceCount(), 1);
        QVERIFY(manager.userDisconnectRequestedForDevice(kDevicePath));

        QTRY_COMPARE_WITH_TIMEOUT(client->connectRequests(), 0, 500);
    }

    void unexpectedDisconnectReconnectsAfterBlueZRestart()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setDevice(kDevicePath, pairedConnectedDevice());

        BluetoothManager manager(client);
        configureFastReconnect(manager);
        QVERIFY(manager.initialize());

        client->updateDevice(kDevicePath, {{QStringLiteral("Connected"), false}}, {});
        QVERIFY(!manager.userDisconnectRequestedForDevice(kDevicePath));

        client->setBlueZAvailable(false);
        QCOMPARE(manager.deviceCount(), 0);

        client->setBlueZAvailable(true);
        client->requestSnapshot();
        QCOMPARE(manager.deviceCount(), 1);

        QTRY_VERIFY_WITH_TIMEOUT(client->connectRequests() >= 1, 500);
    }

    void forgetClearsRetainedReconnectMetadata()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setDevice(kDevicePath, pairedConnectedDevice());

        BluetoothManager manager(client);
        configureFastReconnect(manager);
        QVERIFY(manager.initialize());

        manager.disconnectDevice(kDevicePath);
        client->updateDevice(kDevicePath, {{QStringLiteral("Connected"), false}}, {});
        QVERIFY(manager.userDisconnectRequestedForDevice(kDevicePath));

        manager.forgetDevice(kDevicePath);
        QCOMPARE(manager.deviceCount(), 0);

        client->setDevice(kDevicePath, classicDevice());
        QCOMPARE(manager.deviceCount(), 1);
        QVERIFY(!manager.userDisconnectRequestedForDevice(kDevicePath));
    }

    void manualReconnectClearsDisconnectSuppression()
    {
        auto* client = new FakeBlueZClient;
        addPoweredAdapter(client);
        client->setDevice(kDevicePath, pairedConnectedDevice());

        BluetoothManager manager(client);
        configureFastReconnect(manager);
        QVERIFY(manager.initialize());

        manager.disconnectDevice(kDevicePath);
        client->updateDevice(kDevicePath, {{QStringLiteral("Connected"), false}}, {});
        QVERIFY(manager.userDisconnectRequestedForDevice(kDevicePath));

        manager.reconnectDevice(kDevicePath);
        QVERIFY(!manager.userDisconnectRequestedForDevice(kDevicePath));
        client->updateDevice(kDevicePath, {{QStringLiteral("Connected"), true}}, {});

        client->updateDevice(kDevicePath, {{QStringLiteral("Connected"), false}}, {});
        QTRY_VERIFY_WITH_TIMEOUT(client->connectRequests() >= 1, 500);
    }
};

const QString TstBluetoothManager::kDevicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_01");
const QString TstBluetoothManager::kAdapterPath = QStringLiteral("/org/bluez/hci0");

QTEST_GUILESS_MAIN(TstBluetoothManager)
#include "tst_BluetoothManager.moc"

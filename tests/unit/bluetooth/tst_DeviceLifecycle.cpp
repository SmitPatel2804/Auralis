#include <auralis/bluetooth/AdapterManager.h>
#include <auralis/bluetooth/BlueZAgent.h>
#include <auralis/bluetooth/BlueZPropertyParser.h>
#include <auralis/bluetooth/DeviceLifecycleManager.h>
#include <auralis/bluetooth/DeviceOperation.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/ReconnectPolicy.h>

#include "FakeBlueZClient.h"

#include <QDBusMessage>
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
using auralis::bluetooth::ReconnectPolicyConfig;
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

        void setReconnectConfig(int initialDelayMs = 20, int maxAttempts = 3, int maxDelayMs = 50)
        {
            ReconnectPolicyConfig config;
            config.initialDelayMs = initialDelayMs;
            config.maxAttempts = maxAttempts;
            config.maxDelayMs = maxDelayMs;
            config.backoffMultiplier = 1.0;
            reconnect.setConfig(config);
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

    void cancelPairingTransitionsAndCallsBlueZ()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path));
        h.client.setAutoCompleteDeviceOps(false);
        h.client.setAutoCompleteCancelPairing(false);

        h.lifecycle.pairDevice(path);
        h.lifecycle.cancelPairing(path);

        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::CancellingPairing);
        QCOMPARE(h.client.cancelPairRequests(), 1);
        QCOMPARE(h.client.lastCancelPairPath(), path);
    }

    void duplicateCancelPairingIsSafe()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path));
        h.client.setAutoCompleteDeviceOps(false);
        h.client.setAutoCompleteCancelPairing(false);

        h.lifecycle.pairDevice(path);
        h.lifecycle.cancelPairing(path);
        h.lifecycle.cancelPairing(path);

        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::CancellingPairing);
        QCOMPARE(h.client.cancelPairRequests(), 1);
    }

    void cancelPairingInvalidatesPendingRequest()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path));
        h.client.setAutoCompleteDeviceOps(false);
        h.client.setAutoCompleteCancelPairing(false);

        QDBusMessage call;
        h.agent.handleRequestPinCode(path, call);
        QVERIFY(h.agent.pendingRequest() != nullptr);

        h.lifecycle.pairDevice(path);
        h.lifecycle.cancelPairing(path);

        QVERIFY(h.agent.pendingRequest() == nullptr);
        QCOMPARE(h.client.cancelPairRequests(), 1);
    }

    void cancelPairingRejectedOutsidePairing()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, false, true));
        h.client.setAutoCompleteDeviceOps(false);
        h.client.setAutoCompleteCancelPairing(false);

        h.lifecycle.connectDevice(path);
        h.lifecycle.cancelPairing(path);

        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Connecting);
        QCOMPARE(h.client.cancelPairRequests(), 0);
    }

    void latePairCallbackAfterCancelIsIgnored()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path));
        h.client.setAutoCompleteDeviceOps(false);

        h.lifecycle.pairDevice(path);
        h.lifecycle.cancelPairing(path);
        QCOMPARE(h.client.cancelPairRequests(), 1);
        QCOMPARE(h.client.pairRequests(), 1);

        h.client.completePairSuccess(path);
        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Idle);
        QVERIFY(!device->paired);
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

    void cancelConnectClearsBusyState()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, false, true));
        h.client.setAutoCompleteDeviceOps(false);

        h.lifecycle.connectDevice(path);
        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Connecting);

        h.lifecycle.cancelDeviceOperation(path);
        device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Idle);
        QVERIFY(canConnect(*device));
        QVERIFY(canForget(*device));
        QCOMPARE(h.client.disconnectRequests(), 1);
    }

    void cancelReconnectAllowsForget()
    {
        Harness h;
        h.seedAdapter();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, false, true));
        h.client.setAutoCompleteDeviceOps(false);

        h.lifecycle.reconnectDevice(path);
        h.lifecycle.forgetDevice(path);
        QCOMPARE(h.client.removeRequests(), 0);

        h.lifecycle.cancelDeviceOperation(path);
        h.lifecycle.forgetDevice(path);
        QCOMPARE(h.client.removeRequests(), 1);
    }

    void reconnectFailureSchedulesNextAttempt()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        h.client.setAutoCompleteDeviceOps(false);
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);

        emit h.client.connectDeviceFinished(
            path,
            false,
            QStringLiteral("org.bluez.Error.ConnectionAttemptFailed"),
            QStringLiteral("try again"));

        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 2, 500);
        QCOMPARE(h.client.connectRequests(), 2);
        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Reconnecting);
        QCOMPARE(device->reconnectAttempt, 2);
    }

    void reconnectNonRetryableErrorStopsRetries()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        h.client.setAutoCompleteDeviceOps(false);
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);

        emit h.client.connectDeviceFinished(
            path,
            false,
            QStringLiteral("org.bluez.Error.AuthenticationFailed"),
            QStringLiteral("bad credentials"));

        QTest::qWait(150);
        QCOMPARE(dueSpy.count(), 1);
    }

    void reconnectSuccessResetsAttempts()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        h.client.setAutoCompleteDeviceOps(false);
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);

        h.client.completeConnectSuccess(path);
        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), true}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), true}}, {});

        QCOMPARE(h.reconnect.attempt(path), 0);
        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Idle);
    }

    void blueZRecoveryResumesReconnectForEligibleDevice()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, false, true));
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        h.lifecycle.onBlueZAvailabilityChanged(false);
        h.lifecycle.onBlueZAvailabilityChanged(true);
        h.lifecycle.onSnapshotApplied();

        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);
    }

    void blueZRecoveryKeepsIntentionalDisconnectSuppressed()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, false, true));
        h.registry.setUserDisconnectRequested(path, true);
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        h.lifecycle.onBlueZAvailabilityChanged(false);
        h.lifecycle.onBlueZAvailabilityChanged(true);
        h.lifecycle.onSnapshotApplied();

        QTest::qWait(150);
        QCOMPARE(dueSpy.count(), 0);
    }

    void forgetCancelsScheduledReconnect()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig(100, 3, 100);
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});
        QVERIFY(h.reconnect.isScheduled(path));

        h.lifecycle.forgetDevice(path);
        QVERIFY(!h.reconnect.isScheduled(path));

        QTest::qWait(150);
        QCOMPARE(dueSpy.count(), 0);
    }

    void disconnectPropertyCompletionSurvivesRegistryMutation()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        h.lifecycle.disconnectDevice(path);
        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});

        const BluetoothDeviceData* device = h.registry.findByObjectPath(path);
        QVERIFY(device != nullptr);
        QVERIFY(device->operation == DeviceOperation::Idle);
        QVERIFY(device->userDisconnectRequested);
        QVERIFY(!h.reconnect.isScheduled(path));
        QCOMPARE(dueSpy.count(), 0);
    }

    void suppressAutoReconnectBlocksUnexpectedDisconnect()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        h.lifecycle.suppressAutoReconnect(path);
        QVERIFY(h.lifecycle.isAutoReconnectSuppressed(path));

        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});

        QTest::qWait(150);
        QCOMPARE(dueSpy.count(), 0);
        QVERIFY(!h.reconnect.isScheduled(path));

        h.lifecycle.unsuppressAutoReconnect(path);
        QVERIFY(!h.lifecycle.isAutoReconnectSuppressed(path));
    }

    void suppressAutoReconnectBlocksReevaluation()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, false, true));
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        h.lifecycle.suppressAutoReconnect(path);
        h.lifecycle.onBlueZAvailabilityChanged(false);
        h.lifecycle.onBlueZAvailabilityChanged(true);
        h.lifecycle.onSnapshotApplied();

        QTest::qWait(150);
        QCOMPARE(dueSpy.count(), 0);
    }

    void terminalReconnectFailureEmitsSignal()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig();
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        h.client.setAutoCompleteDeviceOps(false);
        QSignalSpy termSpy(&h.reconnect, &ReconnectPolicy::reconnectTerminalFailure);
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);

        emit h.client.connectDeviceFinished(
            path,
            false,
            QStringLiteral("org.bluez.Error.AuthenticationFailed"),
            QStringLiteral("auth failed"));

        QCOMPARE(termSpy.count(), 1);
        QCOMPARE(termSpy.at(0).at(0).toString(), path);
        QVERIFY(!h.reconnect.isScheduled(path));
        QCOMPARE(h.reconnect.attempt(path), 0);
    }
    void busyOperationDefersReconnect()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig(200, 5, 200);
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        h.client.setAutoCompleteDeviceOps(false);
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        // Unexpected disconnect while Idle triggers reconnect schedule (200ms timer)
        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});
        QVERIFY(h.reconnect.isScheduled(path));

        // Before the timer fires, start a user-initiated connect (occupies pending slot)
        h.lifecycle.connectDevice(path);
        QCOMPARE(h.client.connectRequests(), 1); // user-initiated connect

        // Now wait for the reconnect timer to fire — it should hit contention
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);

        // No additional connect call (contention deferred it)
        QCOMPARE(h.client.connectRequests(), 1);

        // Complete the user-initiated connect successfully — onConnected cancels reconnect
        emit h.client.connectDeviceFinished(path, true, {}, {});
        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), true}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), true}}, {});

        QTest::qWait(100);
        // Reconnect was cancelled by onConnected, no additional connect calls
        QCOMPARE(h.client.connectRequests(), 1);
        QVERIFY(!h.reconnect.isScheduled(path));
    }

    void deferredReconnectCancelled()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig(200, 5, 200);
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        h.client.setAutoCompleteDeviceOps(false);
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        // Unexpected disconnect triggers reconnect schedule
        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});

        // Put device into busy state (user-initiated connect) before timer fires
        h.lifecycle.connectDevice(path);

        // Wait for timer to fire (hits contention)
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);

        // Cancel reconnect while device is busy (session stop scenario)
        h.reconnect.cancelReconnect(path);

        // Complete the user-initiated connect with failure (so onConnected doesn't cancel)
        emit h.client.connectDeviceFinished(
            path, false, QStringLiteral("org.bluez.Error.Failed"), QStringLiteral("failed"));

        QTest::qWait(100);
        // Only the user-initiated connect was called, no reconnect connect
        QCOMPARE(h.client.connectRequests(), 1);
        QVERIFY(!h.reconnect.isScheduled(path));
    }

    void deferredReconnectPolicyChangeToNone()
    {
        Harness h;
        h.seedAdapter();
        h.setReconnectConfig(200, 5, 200);
        const QString path = QStringLiteral("/org/bluez/hci0/dev_AA");
        h.registry.upsertDevice(makeDevice(path, true, true, true));
        h.client.setAutoCompleteDeviceOps(false);
        QSignalSpy dueSpy(&h.reconnect, &ReconnectPolicy::reconnectDue);

        // Unexpected disconnect triggers reconnect schedule
        h.registry.applyPropertyChanges(path, {{QStringLiteral("Connected"), false}}, {});
        h.lifecycle.onDevicePropertiesChanged(path, {{QStringLiteral("Connected"), false}}, {});

        // Put device into busy state (user-initiated connect) before timer fires
        h.lifecycle.connectDevice(path);

        // Wait for timer to fire (hits contention)
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);

        // Disable reconnect policy while deferred
        ReconnectPolicyConfig disabledConfig;
        disabledConfig.enabled = false;
        h.reconnect.setConfig(disabledConfig);
        h.reconnect.cancelAll();

        // Complete the user-initiated connect with failure
        emit h.client.connectDeviceFinished(
            path, false, QStringLiteral("org.bluez.Error.Failed"), QStringLiteral("failed"));

        QTest::qWait(100);
        // Only the user-initiated connect call
        QCOMPARE(h.client.connectRequests(), 1);
    }
};

QTEST_GUILESS_MAIN(TstDeviceLifecycle)
#include "tst_DeviceLifecycle.moc"

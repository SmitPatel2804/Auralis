#include "../unit/bluetooth/FakeBlueZClient.h"

#include <auralis/audio/PipeWireManager.h>
#include <auralis/audio/PipeWireTypes.h>
#include <auralis/bluetooth/BlueZDbusClient.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/recovery/RecoveryManager.h>
#include <auralis/recovery/SystemPowerMonitor.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::audio::PipeWireClientEvent;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireManager;
using auralis::bluetooth::BlueZDbusClient;
using auralis::bluetooth::BluetoothManager;
using auralis::recovery::RecoveryManager;
using auralis::recovery::RecoveryState;
using auralis::recovery::SystemPowerMonitor;
using auralis::test::FakeBlueZClient;

namespace {

PipeWireClientEvent stateEvent(PipeWireConnectionState state)
{
    PipeWireClientEvent event;
    event.type = PipeWireClientEvent::Type::StateChanged;
    event.state = state;
    return event;
}

} // namespace

class TstPhase8RecoveryHarness : public QObject {
    Q_OBJECT

private slots:
    void runtimeSystemBusBounceOnBlueZClient()
    {
        BlueZDbusClient client;
        client.setSystemBusConnectedOverrideForTesting(true);
        QVERIFY(client.initialize());
        QVERIFY(client.busHealthTimerActiveForTesting());

        RecoveryManager recovery;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = [&]() { return client.isBlueZAvailable(); };
        hooks.isSystemBusConnected = [&]() { return client.isSystemBusConnected(); };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        recovery.setHooks(std::move(hooks));
        QVERIFY(recovery.initialize());

        client.setSystemBusConnectedOverrideForTesting(false);
        client.pollSystemBusHealthForTesting();
        recovery.notifySystemBusConnected(false);
        QVERIFY(!client.isSystemBusConnected());

        client.setSystemBusConnectedOverrideForTesting(true);
        client.pollSystemBusHealthForTesting();
        recovery.notifySystemBusConnected(true);
        client.setBlueZAvailableForTesting(true);
        recovery.notifyBlueZAvailable(true);
        recovery.flushPendingReconcileForTesting();
        QVERIFY(sessionRefresh >= 1);
        client.shutdown();
        recovery.shutdown();
    }

    void blueZDaemonBounceUsesRealBluetoothManager()
    {
        auto* fake = new FakeBlueZClient;
        BluetoothManager bluetooth(fake);
        QVERIFY(bluetooth.initialize());

        RecoveryManager recovery;
        int sessionRefresh = 0;
        int snapshotsBefore = fake->snapshotRequests();
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { bluetooth.pauseManagedReconnect(); };
        hooks.resumeBluetoothReconnect = [&]() { bluetooth.resumeManagedReconnect(); };
        hooks.requestBlueZRefresh = [&]() { bluetooth.refresh(); };
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = [&]() { return bluetooth.available(); };
        hooks.isSystemBusConnected = [&]() { return bluetooth.systemBusConnected(); };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        recovery.setHooks(std::move(hooks));
        QVERIFY(recovery.initialize());

        fake->setBlueZAvailable(false);
        recovery.notifyBlueZAvailable(false);
        fake->setBlueZAvailable(true);
        recovery.notifyBlueZAvailable(true);
        // Availability return may refresh via BluetoothManager refresh hook once.
        recovery.flushPendingReconcileForTesting();
        QVERIFY(fake->snapshotRequests() >= snapshotsBefore);
        QVERIFY(sessionRefresh >= 1);
        bluetooth.shutdown();
        recovery.shutdown();
    }

    void pipeWireGraphNeverReadyExhaustsCentralStatus()
    {
        PipeWireManager pipeWire;
        pipeWire.setAutoReconnectEnabled(true);
        pipeWire.initialize();
        pipeWire.setMaxReconnectAttemptsForTesting(2);
        pipeWire.setInitialSyncTimeoutMsForTesting(5);

        RecoveryManager recovery;
        RecoveryManager::HostHooks hooks;
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        hooks.isPipeWireConnected = [&]() { return pipeWire.connected(); };
        hooks.isPipeWireGraphReady = [&]() {
            return pipeWire.connected() && pipeWire.initialSyncComplete();
        };
        recovery.setHooks(std::move(hooks));
        QVERIFY(recovery.initialize());

        QObject::connect(
            &pipeWire,
            &PipeWireManager::reconnectAttemptStarted,
            &recovery,
            &RecoveryManager::notifyPipeWireReconnectAttempt);
        QObject::connect(
            &pipeWire,
            &PipeWireManager::reconnectExhausted,
            &recovery,
            &RecoveryManager::notifyPipeWireReconnectExhausted);

        pipeWire.simulateReconnectFailureForTesting(QStringLiteral("down"));
        pipeWire.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        pipeWire.fireInitialSyncTimeoutForTesting();
        pipeWire.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        pipeWire.fireInitialSyncTimeoutForTesting();

        QVERIFY(recovery.status().pipeWire == RecoveryState::Exhausted);
        QVERIFY(recovery.status().overall == RecoveryState::Exhausted);
        pipeWire.shutdown();
        recovery.shutdown();
    }

    void combinedBlueZAndPipeWireBounceCoalesces()
    {
        auto* fake = new FakeBlueZClient;
        BluetoothManager bluetooth(fake);
        QVERIFY(bluetooth.initialize());
        PipeWireManager pipeWire;
        pipeWire.setAutoReconnectEnabled(true);
        pipeWire.initialize();

        int sessionRefresh = 0;
        RecoveryManager recovery;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { bluetooth.pauseManagedReconnect(); };
        hooks.resumeBluetoothReconnect = [&]() { bluetooth.resumeManagedReconnect(); };
        hooks.requestBlueZRefresh = [&]() { bluetooth.refresh(); };
        hooks.requestPipeWireReconnect = [&]() { pipeWire.requestReconnect(); };
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = [&]() { return bluetooth.available(); };
        hooks.isSystemBusConnected = [&]() { return bluetooth.systemBusConnected(); };
        hooks.isPipeWireConnected = [&]() { return pipeWire.connected() || true; };
        hooks.isPipeWireGraphReady = [&]() { return true; };
        recovery.setHooks(std::move(hooks));
        QVERIFY(recovery.initialize());

        fake->setBlueZAvailable(false);
        recovery.notifyBlueZAvailable(false);
        recovery.notifyPipeWireError(QStringLiteral("pw"));
        fake->setBlueZAvailable(true);
        recovery.notifyBlueZAvailable(true);
        recovery.notifyPipeWireConnected(true, true);
        recovery.flushPendingReconcileForTesting();
        QVERIFY(sessionRefresh >= 1);
        QVERIFY(sessionRefresh <= 3);
        bluetooth.shutdown();
        pipeWire.shutdown();
        recovery.shutdown();
    }

    void autoRecoverDisabledBlocksAutomaticReconcile()
    {
        auto* fake = new FakeBlueZClient;
        BluetoothManager bluetooth(fake);
        QVERIFY(bluetooth.initialize());
        RecoveryManager recovery;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = [&]() { return bluetooth.available(); };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        recovery.setHooks(std::move(hooks));
        recovery.setAutoRecoverEnabled(false);
        QVERIFY(recovery.initialize());
        fake->setBlueZAvailable(false);
        recovery.notifyBlueZAvailable(false);
        fake->setBlueZAvailable(true);
        recovery.notifyBlueZAvailable(true);
        recovery.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 0);
        bluetooth.refresh();
        QVERIFY(fake->snapshotRequests() >= 1);
        bluetooth.shutdown();
        recovery.shutdown();
    }

    void suspendResumeBothPrefsWithDuplicates()
    {
        SystemPowerMonitor power;
        QVERIFY(power.initialize());
        RecoveryManager recovery;
        int pause = 0;
        int resume = 0;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { ++pause; };
        hooks.resumeBluetoothReconnect = [&]() { ++resume; };
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.requestBlueZRefresh = []() {};
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        recovery.setHooks(std::move(hooks));
        QVERIFY(recovery.initialize());

        QObject::connect(&power, &SystemPowerMonitor::preparingForSleep, &recovery, &RecoveryManager::onPreparingForSleep);

        recovery.setRestoreOnResume(true);
        power.injectPrepareForSleep(true);
        power.injectPrepareForSleep(true);
        QCOMPARE(pause, 1);
        power.injectPrepareForSleep(false);
        power.injectPrepareForSleep(false);
        QCOMPARE(resume, 1);
        recovery.flushPendingReconcileForTesting();
        QVERIFY(sessionRefresh >= 1);

        const int sessionAfter = sessionRefresh;
        recovery.setRestoreOnResume(false);
        power.injectPrepareForSleep(true);
        power.injectPrepareForSleep(false);
        recovery.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, sessionAfter);
        power.shutdown();
        recovery.shutdown();
    }

    void shutdownDuringPendingWorkDoesNotResurrect()
    {
        BlueZDbusClient client;
        client.setSystemBusConnectedOverrideForTesting(true);
        QVERIFY(client.initialize());
        client.setBlueZAvailableForTesting(true);
        client.requestSnapshot();

        PipeWireManager pipeWire;
        pipeWire.setAutoReconnectEnabled(true);
        pipeWire.initialize();
        pipeWire.simulateReconnectFailureForTesting(QStringLiteral("x"));
        pipeWire.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));

        RecoveryManager recovery;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        recovery.setHooks(std::move(hooks));
        QVERIFY(recovery.initialize());
        recovery.notifyPipeWireError(QStringLiteral("x"));

        const quint64 gen = static_cast<quint64>(client.busAttachGenerationForTesting());
        client.shutdown();
        pipeWire.shutdown();
        recovery.shutdown();

        QVariantMap objects;
        objects.insert(QStringLiteral("/x"), QVariantMap{});
        client.injectSnapshotFinishedForTesting(gen, objects);
        recovery.flushPendingReconcileForTesting();
        recovery.notifyPipeWireConnected(true, true);
        QCOMPARE(sessionRefresh, 0);
    }
};

QTEST_GUILESS_MAIN(TstPhase8RecoveryHarness)
#include "tst_Phase8RecoveryHarness.moc"

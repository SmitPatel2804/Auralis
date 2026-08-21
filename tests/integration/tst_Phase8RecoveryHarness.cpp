#include "../unit/audio/AudioTestFixtures.h"
#include "../unit/audio/FakePipeWireLinkBackend.h"
#include "../unit/bluetooth/FakeBlueZClient.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/PipeWireManager.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/audio/PipeWireTypes.h>
#include <auralis/bluetooth/BlueZDbusClient.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/recovery/RecoveryManager.h>
#include <auralis/recovery/SystemPowerMonitor.h>
#include <auralis/session/AuralisSession.h>
#include <auralis/session/RoutingCoordinator.h>
#include <auralis/session/SessionTypes.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::audio::AudioEndpointRegistry;
using auralis::audio::AudioRouter;
using auralis::audio::PipeWireClientEvent;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireManager;
using auralis::audio::PipeWireObjectStore;
using auralis::audio::RouteState;
using auralis::bluetooth::BlueZDbusClient;
using auralis::bluetooth::BluetoothManager;
using auralis::bluetooth::DeviceRegistry;
using auralis::recovery::RecoveryManager;
using auralis::recovery::RecoveryState;
using auralis::recovery::SystemPowerMonitor;
using auralis::session::AuralisSession;
using auralis::session::RoutingCoordinator;
using auralis::session::SessionDevice;
using auralis::session::SessionState;
using auralis::test::FakeBlueZClient;
using auralis::test::FakePipeWireLinkBackend;
using auralis::test::makeBluetoothDevice;
using auralis::test::makeNode;
using auralis::test::makePlaybackEndpoint;
using auralis::test::makePort;

namespace {

PipeWireClientEvent stateEvent(PipeWireConnectionState state)
{
    PipeWireClientEvent event;
    event.type = PipeWireClientEvent::Type::StateChanged;
    event.state = state;
    return event;
}

PipeWireClientEvent syncDoneEvent()
{
    PipeWireClientEvent event;
    event.type = PipeWireClientEvent::Type::InitialSyncDone;
    event.state = PipeWireConnectionState::Connected;
    return event;
}

struct RouteHarness {
    PipeWireObjectStore store;
    AudioEndpointRegistry endpoints;
    DeviceRegistry devices;
    FakePipeWireLinkBackend backend{&store};
    AudioRouter router{&store, &endpoints, &backend};
    RoutingCoordinator coordinator{&router, &endpoints, &devices};

    RouteHarness()
    {
        router.handleConnectionState(PipeWireConnectionState::Connected, true);
    }

    void addSource()
    {
        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("7")}}));
        store.upsert(makePort(11, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(12, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        router.refreshSources();
    }

    void addMember(const QString& address, quint32 nodeId, quint32 inFl, quint32 inFr, const QString& endpointId)
    {
        devices.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_%1").arg(QString(address).remove(':')),
            address,
            endpointId,
            true));
        store.upsert(makeNode(nodeId, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                       {QStringLiteral("node.name"), endpointId}}));
        store.upsert(makePort(inFl, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(inFr, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        auto endpoint = makePlaybackEndpoint(endpointId, nodeId, endpointId);
        endpoint.bluetoothAddress = address;
        endpoints.upsert(endpoint);
    }

    void rebindMemberNode(
        const QString& address,
        quint32 oldNodeId,
        quint32 oldInFl,
        quint32 oldInFr,
        quint32 newNodeId,
        quint32 inFl,
        quint32 inFr,
        const QString& endpointId)
    {
        store.remove(oldInFl);
        store.remove(oldInFr);
        store.remove(oldNodeId);
        store.upsert(makeNode(newNodeId, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                          {QStringLiteral("node.name"), endpointId}}));
        store.upsert(makePort(inFl, newNodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(inFr, newNodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        auto endpoint = makePlaybackEndpoint(endpointId, newNodeId, endpointId);
        endpoint.bluetoothAddress = address;
        endpoints.upsert(endpoint);
        router.handleGraphChanged();
    }
};

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
        recovery.notifyPipeWireConnected(true, true);
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
        const int snapshotsBefore = fake->snapshotRequests();
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
        recovery.notifyPipeWireConnected(true, true);

        fake->setBlueZAvailable(false);
        recovery.notifyBlueZAvailable(false);
        fake->setBlueZAvailable(true);
        recovery.notifyBlueZAvailable(true);
        recovery.flushPendingReconcileForTesting();
        QVERIFY(fake->snapshotRequests() >= snapshotsBefore);
        QCOMPARE(sessionRefresh, 1);
        bluetooth.shutdown();
        recovery.shutdown();
    }

    void blueZReturnRequestsExactlyOneSnapshotAndOneSessionRefresh()
    {
        auto* fake = new FakeBlueZClient;
        BluetoothManager bluetooth(fake);
        QVERIFY(bluetooth.initialize());

        RecoveryManager recovery;
        int sessionRefresh = 0;
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
        recovery.notifyPipeWireConnected(true, true);

        const int snapshotsBefore = fake->snapshotRequests();
        fake->setBlueZAvailable(false);
        recovery.notifyBlueZAvailable(false);
        fake->setBlueZAvailable(true);
        recovery.notifyBlueZAvailable(true);
        recovery.flushPendingReconcileForTesting();
        QCOMPARE(fake->snapshotRequests(), snapshotsBefore + 1);
        QCOMPARE(sessionRefresh, 1);
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
        hooks.isPipeWireConnected = [&]() { return pipeWire.connected(); };
        hooks.isPipeWireGraphReady = [&]() { return pipeWire.connected() && pipeWire.initialSyncComplete(); };
        recovery.setHooks(std::move(hooks));
        QVERIFY(recovery.initialize());

        fake->setBlueZAvailable(false);
        recovery.notifyBlueZAvailable(false);
        recovery.notifyPipeWireError(QStringLiteral("pw"));
        fake->setBlueZAvailable(true);
        recovery.notifyBlueZAvailable(true);
        pipeWire.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        pipeWire.injectClientEventForTesting(syncDoneEvent());
        recovery.notifyPipeWireConnected(true, true);
        recovery.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 1);
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
        hooks.isSystemBusConnected = []() { return true; };
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
        hooks.isSystemBusConnected = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        recovery.setHooks(std::move(hooks));
        QVERIFY(recovery.initialize());
        recovery.notifyPipeWireConnected(true, true);

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

    void stableEndpointRebindRecreatesRouteOnce()
    {
        RouteHarness h;
        h.addSource();
        const QString address = QStringLiteral("AA:BB:CC:DD:EE:42");
        const QString endpointId = QStringLiteral("dest-rebind");
        h.addMember(address, 42, 421, 422, endpointId);

        AuralisSession session;
        session.id = QStringLiteral("sess-rebind");
        session.sourceId = QStringLiteral("src:7:Stream/Output/Audio");
        session.state = SessionState::Active;
        SessionDevice member;
        member.deviceId = address;
        session.devices = {member};
        session.operationGeneration = 1;
        QHash<QString, quint64> generations{{session.id, 1}};

        int reconcileCount = 0;
        RecoveryManager recovery;
        RecoveryManager::HostHooks hooks;
        hooks.refreshActiveSession = [&]() {
            ++reconcileCount;
            h.coordinator.reconcile(session, 1, generations);
        };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        recovery.setHooks(std::move(hooks));
        QVERIFY(recovery.initialize());
        recovery.notifyPipeWireConnected(true, true);
        recovery.notifyBlueZAvailable(false);
        recovery.notifyBlueZAvailable(true);
        recovery.flushPendingReconcileForTesting();
        QCOMPARE(reconcileCount, 1);
        QVERIFY(session.devices.front().runtime.routeActive);
        const QString routeId = session.devices.front().runtime.routeId;
        QVERIFY(!routeId.isEmpty());
        const int createsAfterFirst = h.backend.createCalls;
        QVERIFY(createsAfterFirst >= 2);

        h.rebindMemberNode(address, 42, 421, 422, 108, 1081, 1082, endpointId);
        const auto afterRebind = h.router.routeById(routeId);
        QVERIFY(afterRebind.has_value());
        QVERIFY(afterRebind->state == RouteState::Active || afterRebind->state == RouteState::Activating
                || afterRebind->state == RouteState::Degraded);
        QVERIFY(h.backend.createCalls > createsAfterFirst);
        const int createsAfterRebind = h.backend.createCalls;

        recovery.notifyBlueZAvailable(false);
        recovery.notifyBlueZAvailable(true);
        recovery.flushPendingReconcileForTesting();
        QCOMPARE(reconcileCount, 2);
        QCOMPARE(session.devices.front().runtime.routeId, routeId);
        QCOMPARE(h.backend.createCalls, createsAfterRebind);
        recovery.shutdown();
    }

    void multiDeviceMemberChurnBoundsReconcile()
    {
        RouteHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"));
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, 31, 32, QStringLiteral("dest-b"));

        AuralisSession session;
        session.id = QStringLiteral("sess-churn");
        session.sourceId = QStringLiteral("src:7:Stream/Output/Audio");
        session.state = SessionState::Active;
        SessionDevice left;
        left.deviceId = QStringLiteral("AA:BB:CC:DD:EE:01");
        SessionDevice right;
        right.deviceId = QStringLiteral("AA:BB:CC:DD:EE:02");
        session.devices = {left, right};
        session.operationGeneration = 1;
        QHash<QString, quint64> generations{{session.id, 1}};

        auto* fake = new FakeBlueZClient;
        BluetoothManager bluetooth(fake);
        QVERIFY(bluetooth.initialize());

        int reconcileCount = 0;
        RecoveryManager recovery;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { bluetooth.pauseManagedReconnect(); };
        hooks.resumeBluetoothReconnect = [&]() { bluetooth.resumeManagedReconnect(); };
        hooks.requestBlueZRefresh = [&]() { bluetooth.refresh(); };
        hooks.refreshActiveSession = [&]() {
            ++reconcileCount;
            h.coordinator.reconcile(session, 1, generations);
        };
        hooks.isBlueZAvailable = [&]() { return bluetooth.available(); };
        hooks.isSystemBusConnected = [&]() { return bluetooth.systemBusConnected(); };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        recovery.setHooks(std::move(hooks));
        QVERIFY(recovery.initialize());
        recovery.notifyPipeWireConnected(true, true);

        recovery.notifyBlueZAvailable(false);
        recovery.notifyBlueZAvailable(true);
        recovery.flushPendingReconcileForTesting();
        QVERIFY(session.devices.at(0).runtime.routeActive);
        QVERIFY(session.devices.at(1).runtime.routeActive);

        session.devices[0].enabled = false;
        recovery.notifyBlueZAvailable(false);
        recovery.notifyBlueZAvailable(true);
        recovery.flushPendingReconcileForTesting();
        QVERIFY(session.devices.at(0).runtime.routeId.isEmpty());
        QVERIFY(session.devices.at(1).runtime.routeActive);

        session.devices[0].enabled = true;
        session.devices[1].enabled = false;
        recovery.notifyBlueZAvailable(false);
        recovery.notifyBlueZAvailable(true);
        recovery.flushPendingReconcileForTesting();
        QVERIFY(session.devices.at(0).runtime.routeActive);
        QVERIFY(session.devices.at(1).runtime.routeId.isEmpty());

        QVERIFY(reconcileCount <= 4);
        QVERIFY(reconcileCount >= 3);
        bluetooth.shutdown();
        recovery.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstPhase8RecoveryHarness)
#include "tst_Phase8RecoveryHarness.moc"

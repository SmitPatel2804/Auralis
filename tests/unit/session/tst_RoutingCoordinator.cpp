#include "../audio/AudioTestFixtures.h"
#include "../audio/FakePipeWireLinkBackend.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/bluetooth/BlueZPropertyParser.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/session/RoutingCoordinator.h>
#include <auralis/session/VolumeCoordinator.h>

#include <QtTest>

using auralis::audio::AudioEndpointRegistry;
using auralis::audio::AudioRouter;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireObjectStore;
using auralis::audio::RouteState;
using auralis::session::AuralisSession;
using auralis::session::RoutingCoordinator;
using auralis::session::SessionDevice;
using auralis::session::SessionState;
using auralis::session::VolumeCoordinator;
using auralis::test::FakePipeWireLinkBackend;
using auralis::test::makeLink;
using auralis::test::makeNode;
using auralis::test::makePlaybackEndpoint;
using auralis::test::makePort;
using auralis::test::makeBluetoothDevice;

class TstRoutingCoordinator : public QObject {
    Q_OBJECT

private:
    struct Harness {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        auralis::bluetooth::DeviceRegistry devices;
        FakePipeWireLinkBackend backend{&store};
        AudioRouter router{&store, &endpoints, &backend};
        RoutingCoordinator coordinator{&router, &endpoints, &devices};

        Harness()
        {
            router.handleConnectionState(PipeWireConnectionState::Connected, true);
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

        void addSource()
        {
            store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                      {QStringLiteral("object.serial"), QStringLiteral("7")}}));
            store.upsert(makePort(11, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
            store.upsert(makePort(12, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
            router.refreshSources();
        }
    };

private slots:
    void twoMembersCreateTwoRoutes()
    {
        Harness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"));
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, 31, 32, QStringLiteral("dest-b"));

        AuralisSession session;
        session.id = QStringLiteral("sess-1");
        session.sourceId = QStringLiteral("src:7:Stream/Output/Audio");
        SessionDevice left;
        left.deviceId = QStringLiteral("AA:BB:CC:DD:EE:01");
        SessionDevice right;
        right.deviceId = QStringLiteral("AA:BB:CC:DD:EE:02");
        session.devices = {left, right};
        session.operationGeneration = 1;
        QHash<QString, quint64> generations{{session.id, 1}};

        h.coordinator.reconcile(session, 1, generations);
        QCOMPARE(session.devices.size(), 2);
        QVERIFY(!session.devices.at(0).runtime.routeId.isEmpty());
        QVERIFY(!session.devices.at(1).runtime.routeId.isEmpty());
        QVERIFY(session.devices.at(0).runtime.routeId != session.devices.at(1).runtime.routeId);
    }

    void repeatedReconcileDoesNotDuplicateRoutes()
    {
        Harness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"));
        AuralisSession session;
        session.id = QStringLiteral("sess-1");
        session.sourceId = QStringLiteral("src:7:Stream/Output/Audio");
        SessionDevice left;
        left.deviceId = QStringLiteral("AA:BB:CC:DD:EE:01");
        session.devices = {left};
        session.operationGeneration = 1;
        QHash<QString, quint64> generations{{session.id, 1}};
        h.coordinator.reconcile(session, 1, generations);
        const QString firstRoute = session.devices.front().runtime.routeId;
        h.coordinator.reconcile(session, 1, generations);
        QCOMPARE(session.devices.front().runtime.routeId, firstRoute);
        QCOMPARE(h.backend.createCalls, 2);
    }

    void stopClearsOwnedRoutesOnly()
    {
        Harness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"));
        AuralisSession session;
        session.id = QStringLiteral("sess-1");
        session.sourceId = QStringLiteral("src:7:Stream/Output/Audio");
        SessionDevice left;
        left.deviceId = QStringLiteral("AA:BB:CC:DD:EE:01");
        session.devices = {left};
        session.operationGeneration = 1;
        QHash<QString, quint64> generations{{session.id, 1}};
        h.coordinator.reconcile(session, 1, generations);

        h.store.upsert(makeLink(50, 1, 11, 99, 91, QStringLiteral("active")));
        h.coordinator.stopSessionRoutes(session);
        QVERIFY(session.devices.front().runtime.routeId.isEmpty());
        QCOMPARE(h.router.ownedLinkCount(), 0);
    }

    void policyNoneDoesNotReactivateExistingRoute()
    {
        Harness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"));
        AuralisSession session;
        session.id = QStringLiteral("sess-none");
        session.sourceId = QStringLiteral("src:7:Stream/Output/Audio");
        session.state = SessionState::Active;
        SessionDevice left;
        left.deviceId = QStringLiteral("AA:BB:CC:DD:EE:01");
        left.runtime.autoRestoreAllowed = false;
        session.devices = {left};
        session.operationGeneration = 1;
        QHash<QString, quint64> generations{{session.id, 1}};
        left.runtime.autoRestoreAllowed = true;
        session.devices = {left};
        h.coordinator.reconcile(session, 1, generations);
        QVERIFY(session.devices.front().runtime.routeActive);
        const QString routeId = session.devices.front().runtime.routeId;
        const int creates = h.backend.createCalls;
        h.router.deactivateRoute(routeId);
        session.devices.front().runtime.autoRestoreAllowed = false;
        session.state = SessionState::Degraded;
        h.coordinator.reconcile(session, 1, generations);
        QCOMPARE(h.backend.createCalls, creates);
        const auto route = h.router.routeById(routeId);
        QVERIFY(route.has_value());
        QVERIFY(route->state != RouteState::Active);
    }

    void staleGenerationReconcileIsNoOp()
    {
        Harness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"));
        AuralisSession session;
        session.id = QStringLiteral("sess-stale");
        session.sourceId = QStringLiteral("src:7:Stream/Output/Audio");
        SessionDevice left;
        left.deviceId = QStringLiteral("AA:BB:CC:DD:EE:01");
        session.devices = {left};
        session.operationGeneration = 1;
        QHash<QString, quint64> generations{{session.id, 1}};
        h.coordinator.reconcile(session, 1, generations);
        QVERIFY(!session.devices.front().runtime.routeId.isEmpty());
        const int creates = h.backend.createCalls;
        const QString routeId = session.devices.front().runtime.routeId;

        h.coordinator.reconcile(session, 0, generations);
        QCOMPARE(h.backend.createCalls, creates);
        QCOMPARE(session.devices.front().runtime.routeId, routeId);
    }

    void failedRouteIsNotRetriedUntilStarting()
    {
        Harness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"));
        AuralisSession session;
        session.id = QStringLiteral("sess-failed");
        session.sourceId = QStringLiteral("src:7:Stream/Output/Audio");
        session.state = SessionState::Active;
        SessionDevice left;
        left.deviceId = QStringLiteral("AA:BB:CC:DD:EE:01");
        session.devices = {left};
        session.operationGeneration = 1;
        QHash<QString, quint64> generations{{session.id, 1}};
        h.coordinator.reconcile(session, 1, generations);
        const QString routeId = session.devices.front().runtime.routeId;
        QVERIFY(!routeId.isEmpty());
        const int creates = h.backend.createCalls;

        if (auto* route = h.router.mutableRoute(routeId)) {
            route->state = RouteState::Failed;
        }
        session.devices.front().runtime.routeActive = false;
        h.coordinator.reconcile(session, 1, generations);
        QCOMPARE(h.backend.createCalls, creates);
        QCOMPARE(session.devices.front().runtime.routeId, routeId);

        session.state = SessionState::Starting;
        h.coordinator.reconcile(session, 1, generations);
        QCOMPARE(h.backend.createCalls, creates);
        QCOMPARE(session.devices.front().runtime.routeId, routeId);

        h.router.removeRoute(routeId);
        session.devices.front().runtime.routeId.clear();
        session.devices.front().runtime.routeRequested = false;
        h.coordinator.reconcile(session, 1, generations);
        QVERIFY(session.devices.front().runtime.routeId != routeId);
        QVERIFY(h.backend.createCalls > creates);
    }

    void groupAndTrimCombine()
    {
        AuralisSession session;
        session.groupVolume = 0.5;
        SessionDevice device;
        device.volumeTrim = 0.8;
        QCOMPARE(VolumeCoordinator::effectiveVolume(session, device), 0.4);
    }
};

QTEST_GUILESS_MAIN(TstRoutingCoordinator)
#include "tst_RoutingCoordinator.moc"

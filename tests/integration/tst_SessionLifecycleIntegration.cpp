#include "../unit/audio/AudioTestFixtures.h"
#include "../unit/audio/FakePipeWireLinkBackend.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/session/SessionManager.h>

#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QtTest>

using auralis::audio::AudioEndpointRegistry;
using auralis::audio::AudioRouter;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireObjectStore;
using auralis::session::SessionManager;
using auralis::session::SessionState;
using auralis::test::FakePipeWireLinkBackend;
using auralis::test::makeBluetoothDevice;
using auralis::test::makeNode;
using auralis::test::makePlaybackEndpoint;
using auralis::test::makePort;

class TstSessionLifecycleIntegration : public QObject {
    Q_OBJECT

private slots:
    void twoDeviceHappyPathAndStopDuringRecovery()
    {
        QTemporaryDir tempDir;
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        auralis::bluetooth::DeviceRegistry devices;
        FakePipeWireLinkBackend backend(&store);
        AudioRouter router(&store, &endpoints, &backend);
        SessionManager manager(
            &router,
            &endpoints,
            &devices,
            nullptr,
            tempDir.filePath(QStringLiteral("sessions.json")));
        manager.initialize();
        router.handleConnectionState(PipeWireConnectionState::Connected, true);

        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("7")}}));
        store.upsert(makePort(11, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(12, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        router.refreshSources();

        auto addMember = [&](const QString& address, quint32 nodeId, const QString& endpointId, bool connected) {
            devices.upsertDevice(makeBluetoothDevice(
                QStringLiteral("/org/bluez/hci0/dev_%1").arg(QString(address).remove(':')), address, endpointId, connected));
            store.upsert(makeNode(nodeId, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                           {QStringLiteral("node.name"), endpointId}}));
            store.upsert(makePort(nodeId * 10 + 1, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
            store.upsert(makePort(nodeId * 10 + 2, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
            auto endpoint = makePlaybackEndpoint(endpointId, nodeId, endpointId);
            endpoint.bluetoothAddress = address;
            endpoints.upsert(endpoint);
        };
        addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, QStringLiteral("dest-a"), true);
        addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, QStringLiteral("dest-b"), true);

        const QString id = manager.createSession(QStringLiteral("Living Room"));
        manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("Left"));
        manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("Right"));
        manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        manager.activateSession(id);

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 2000) {
            const auto session = manager.sessionById(id);
            if (session.has_value() && session->state == SessionState::Active) {
                break;
            }
            QTest::qWait(20);
        }
        QVERIFY(manager.sessionById(id)->state == SessionState::Active);

        devices.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AABBCCDDEE02"),
            QStringLiteral("AA:BB:CC:DD:EE:02"),
            QStringLiteral("dest-b"),
            false));
        endpoints.removeById(QStringLiteral("dest-b"));
        manager.refreshActiveSession();
        QTRY_VERIFY_WITH_TIMEOUT(
            manager.sessionById(id)->state == SessionState::Degraded
                || manager.sessionById(id)->state == SessionState::Recovering,
            2000);

        manager.deactivateSession(id);
        QVERIFY(manager.sessionById(id)->state == SessionState::Idle);
        QCOMPARE(router.ownedLinkCount(), 0);
    }

    void memberReturnsWithNewEndpointId()
    {
        QTemporaryDir tempDir;
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        auralis::bluetooth::DeviceRegistry devices;
        FakePipeWireLinkBackend backend(&store);
        AudioRouter router(&store, &endpoints, &backend);
        SessionManager manager(
            &router,
            &endpoints,
            &devices,
            nullptr,
            tempDir.filePath(QStringLiteral("sessions.json")));
        manager.initialize();
        router.handleConnectionState(PipeWireConnectionState::Connected, true);

        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("7")}}));
        store.upsert(makePort(11, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(12, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        router.refreshSources();

        auto addMember = [&](const QString& address, quint32 nodeId, const QString& endpointId, bool connected) {
            devices.upsertDevice(makeBluetoothDevice(
                QStringLiteral("/org/bluez/hci0/dev_%1").arg(QString(address).remove(':')), address, endpointId, connected));
            store.upsert(makeNode(nodeId, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                           {QStringLiteral("node.name"), endpointId}}));
            store.upsert(makePort(nodeId * 10 + 1, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
            store.upsert(makePort(nodeId * 10 + 2, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
            auto endpoint = makePlaybackEndpoint(endpointId, nodeId, endpointId);
            endpoint.bluetoothAddress = address;
            endpoints.upsert(endpoint);
        };
        addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, QStringLiteral("dest-a"), true);
        addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, QStringLiteral("dest-b"), true);

        const QString id = manager.createSession(QStringLiteral("Return"));
        manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("Left"));
        manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("Right"));
        manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        manager.setRecoveryPolicy(id, QStringLiteral("RestoreRoutesOnly"));
        manager.setGroupVolume(id, 0.5);
        manager.setDeviceMuted(id, QStringLiteral("AA:BB:CC:DD:EE:02"), true);
        manager.activateSession(id);
        QTRY_VERIFY_WITH_TIMEOUT(manager.sessionById(id)->state == SessionState::Active, 2000);
        const QString peerRoute = manager.sessionById(id)->devices.front().runtime.routeId;

        endpoints.removeById(QStringLiteral("dest-b"));
        manager.refreshActiveSession();
        QTRY_VERIFY_WITH_TIMEOUT(
            manager.sessionById(id)->state == SessionState::Degraded
                || manager.sessionById(id)->state == SessionState::Recovering,
            2000);
        QVERIFY(manager.sessionById(id)->devices.front().runtime.routeActive);

        addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 8, QStringLiteral("dest-b-new"), true);
        manager.refreshActiveSession();
        QTRY_VERIFY_WITH_TIMEOUT(manager.sessionById(id)->state == SessionState::Active, 2000);
        QCOMPARE(manager.sessionById(id)->devices.front().runtime.routeId, peerRoute);
        QVERIFY(manager.sessionById(id)->devices.at(1).runtime.routeActive);
        QCOMPARE(manager.sessionById(id)->devices.at(1).runtime.endpointId, QStringLiteral("dest-b-new"));
        QVERIFY(backend.lastMuted);
        QCOMPARE(backend.lastVolume, 0.5);
    }
};

QTEST_GUILESS_MAIN(TstSessionLifecycleIntegration)
#include "tst_SessionLifecycleIntegration.moc"

#include "../audio/AudioTestFixtures.h"
#include "../audio/FakePipeWireLinkBackend.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/ServiceStatus.h>
#include <auralis/session/SessionManager.h>

#include <QTemporaryDir>
#include <QtTest>

using auralis::audio::AudioEndpointRegistry;
using auralis::audio::AudioRouter;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireObjectStore;
using auralis::session::SessionCommandResult;
using auralis::session::SessionManager;
using auralis::session::SessionState;
using auralis::test::FakePipeWireLinkBackend;
using auralis::test::makeBluetoothDevice;
using auralis::test::makeNode;
using auralis::test::makePlaybackEndpoint;
using auralis::test::makePort;

class TstSessionManager : public QObject {
    Q_OBJECT

private:
    struct ActiveHarness {
        QTemporaryDir tempDir;
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        auralis::bluetooth::DeviceRegistry devices;
        FakePipeWireLinkBackend backend{&store};
        AudioRouter router{&store, &endpoints, &backend};
        SessionManager manager;

        ActiveHarness()
            : manager(&router, &endpoints, &devices, nullptr, tempDir.filePath(QStringLiteral("sessions.json")))
        {
            router.handleConnectionState(PipeWireConnectionState::Connected, true);
            manager.initialize();
        }

        void addSource()
        {
            store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                      {QStringLiteral("object.serial"), QStringLiteral("7")}}));
            store.upsert(makePort(11, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
            store.upsert(makePort(12, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
            router.refreshSources();
        }

        void addMember(const QString& address, quint32 nodeId, quint32 inFl, quint32 inFr, const QString& endpointId, bool connected)
        {
            devices.upsertDevice(makeBluetoothDevice(
                QStringLiteral("/org/bluez/hci0/dev_%1").arg(QString(address).remove(':')), address, endpointId, connected));
            store.upsert(makeNode(nodeId, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                           {QStringLiteral("node.name"), endpointId}}));
            store.upsert(makePort(inFl, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
            store.upsert(makePort(inFr, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
            auto endpoint = makePlaybackEndpoint(endpointId, nodeId, endpointId);
            endpoint.bluetoothAddress = address;
            endpoints.upsert(endpoint);
        }
    };

private slots:
    void lifecycleWithoutDependencies()
    {
        QTemporaryDir dir;
        SessionManager manager(nullptr, nullptr, dir.filePath(QStringLiteral("sessions.json")));
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
        QVERIFY(manager.initialize());
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Ready);
        manager.shutdown();
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
    }

    void createRenameDeleteCrud()
    {
        QTemporaryDir dir;
        SessionManager manager(nullptr, nullptr, dir.filePath(QStringLiteral("sessions.json")));
        manager.initialize();
        const QString id = manager.createSession(QStringLiteral("Living Room"));
        QVERIFY(!id.isEmpty());
        QCOMPARE(manager.sessionCount(), 1);
        QVERIFY(manager.sessionById(id)->state == SessionState::Idle);
        QVERIFY(manager.renameSession(id, QStringLiteral("Office")) == SessionCommandResult::Accepted);
        QVERIFY(manager.renameSession(id, QString()) == SessionCommandResult::InvalidSessionName);
        QVERIFY(manager.deleteSession(id) == SessionCommandResult::Accepted);
        QCOMPARE(manager.sessionCount(), 0);
    }

    void duplicateMemberRejected()
    {
        QTemporaryDir dir;
        SessionManager manager(nullptr, nullptr, dir.filePath(QStringLiteral("sessions.json")));
        manager.initialize();
        const QString id = manager.createSession(QStringLiteral("Test"));
        QVERIFY(manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01")) == SessionCommandResult::Accepted);
        QVERIFY(manager.addDevice(id, QStringLiteral("aa:bb:cc:dd:ee:01")) == SessionCommandResult::DuplicateMember);
    }

    void twoDeviceActivation()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, 31, 32, QStringLiteral("dest-b"), true);
        const QString id = h.manager.createSession(QStringLiteral("Pair"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("Left"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("Right"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        QVERIFY(h.manager.activateSession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
        QCOMPARE(h.manager.sessionById(id)->devices.size(), 2);
        h.manager.deactivateSession(id);
        QVERIFY(h.manager.sessionById(id)->state == SessionState::Idle);
        QCOMPARE(h.router.ownedLinkCount(), 0);
    }

    void oneUnavailableMemberStartsDegraded()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        const QString id = h.manager.createSession(QStringLiteral("Partial"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:99"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        h.manager.activateSession(id);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Degraded, 2000);
        QVERIFY(h.manager.sessionById(id)->devices.front().runtime.routeActive);
    }

    void persistenceReloadStartsIdle()
    {
        QTemporaryDir dir;
        {
            SessionManager manager(nullptr, nullptr, dir.filePath(QStringLiteral("sessions.json")));
            manager.initialize();
            const QString id = manager.createSession(QStringLiteral("Saved"));
            manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
            manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
            manager.setGroupVolume(id, 0.6);
        }
        SessionManager manager(nullptr, nullptr, dir.filePath(QStringLiteral("sessions.json")));
        manager.initialize();
        QCOMPARE(manager.sessionCount(), 1);
        QVERIFY(manager.sessions().front().state == SessionState::Idle);
        QCOMPARE(manager.sessions().front().groupVolume, 0.6);
    }
};

QTEST_GUILESS_MAIN(TstSessionManager)
#include "tst_SessionManager.moc"

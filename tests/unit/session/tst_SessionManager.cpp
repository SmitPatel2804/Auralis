#include "../audio/AudioTestFixtures.h"
#include "../audio/FakePipeWireLinkBackend.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/core/ServiceStatus.h>
#include <auralis/session/SessionManager.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <limits>

using auralis::audio::AudioEndpointRegistry;
using auralis::audio::AudioRouter;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireObjectStore;
using auralis::session::SessionCommandResult;
using auralis::session::SessionError;
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

    void recoveryPolicyNoneDoesNotRestoreReturningMember()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, 31, 32, QStringLiteral("dest-b"), true);
        const QString id = h.manager.createSession(QStringLiteral("None"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:02"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        h.manager.setRecoveryPolicy(id, QStringLiteral("None"));
        h.manager.setAutoReconnect(id, false);
        QVERIFY(h.manager.activateSession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
        const QString peerRoute = h.manager.sessionById(id)->devices.front().runtime.routeId;

        h.devices.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AABBCCDDEE02"),
            QStringLiteral("AA:BB:CC:DD:EE:02"),
            QStringLiteral("dest-b"),
            false));
        h.endpoints.removeById(QStringLiteral("dest-b"));
        h.manager.refreshActiveSession();
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Degraded, 2000);
        QCOMPARE(h.manager.sessionById(id)->devices.front().runtime.routeId, peerRoute);
        QVERIFY(h.manager.sessionById(id)->devices.front().runtime.routeActive);

        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, 31, 32, QStringLiteral("dest-b"), true);
        h.manager.refreshActiveSession();
        QVERIFY(h.manager.sessionById(id)->state == SessionState::Degraded);
        QVERIFY(!h.manager.sessionById(id)->devices.at(1).runtime.routeActive);
        QVERIFY(h.manager.takeManagedReconnectRequestsForTest().isEmpty());
    }

    void restoreRoutesOnlyRestoresWithoutBluetoothReconnect()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, 31, 32, QStringLiteral("dest-b"), true);
        const QString id = h.manager.createSession(QStringLiteral("Routes"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:02"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        h.manager.setRecoveryPolicy(id, QStringLiteral("RestoreRoutesOnly"));
        h.manager.setAutoReconnect(id, false);
        h.manager.setGroupVolume(id, 0.5);
        h.manager.setDeviceVolume(id, QStringLiteral("AA:BB:CC:DD:EE:02"), 0.8);
        QVERIFY(h.manager.activateSession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
        const QString peerRoute = h.manager.sessionById(id)->devices.front().runtime.routeId;

        h.devices.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AABBCCDDEE02"),
            QStringLiteral("AA:BB:CC:DD:EE:02"),
            QStringLiteral("dest-b"),
            false));
        h.endpoints.removeById(QStringLiteral("dest-b"));
        h.manager.refreshActiveSession();
        QTRY_VERIFY_WITH_TIMEOUT(
            h.manager.sessionById(id)->state == SessionState::Degraded
                || h.manager.sessionById(id)->state == SessionState::Recovering,
            2000);
        QVERIFY(h.manager.takeManagedReconnectRequestsForTest().isEmpty());

        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 13, 131, 132, QStringLiteral("dest-b2"), true);
        h.manager.refreshActiveSession();
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
        QCOMPARE(h.manager.sessionById(id)->devices.front().runtime.routeId, peerRoute);
        QVERIFY(h.manager.sessionById(id)->devices.at(1).runtime.routeActive);
        QCOMPARE(h.manager.sessionById(id)->devices.at(1).runtime.endpointId, QStringLiteral("dest-b2"));
        QCOMPARE(h.backend.lastVolume, 0.4);
        QVERIFY(h.manager.takeManagedReconnectRequestsForTest().isEmpty());
    }

    void reconnectAndRestoreRequestsManagedReconnectOnce()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, 31, 32, QStringLiteral("dest-b"), true);
        const QString id = h.manager.createSession(QStringLiteral("Reconnect"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:02"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        h.manager.setRecoveryPolicy(id, QStringLiteral("ReconnectAndRestore"));
        QVERIFY(h.manager.activateSession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);

        h.devices.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AABBCCDDEE02"),
            QStringLiteral("AA:BB:CC:DD:EE:02"),
            QStringLiteral("dest-b"),
            false));
        h.endpoints.removeById(QStringLiteral("dest-b"));
        h.manager.refreshActiveSession();
        QTRY_VERIFY_WITH_TIMEOUT(
            h.manager.sessionById(id)->state == SessionState::Degraded
                || h.manager.sessionById(id)->state == SessionState::Recovering,
            2000);
        const QVector<QString> first = h.manager.takeManagedReconnectRequestsForTest();
        QCOMPARE(first.size(), 1);
        h.manager.refreshActiveSession();
        QVERIFY(h.manager.takeManagedReconnectRequestsForTest().isEmpty());

        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 13, 131, 132, QStringLiteral("dest-b2"), true);
        h.manager.refreshActiveSession();
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
        QVERIFY(h.manager.sessionById(id)->devices.at(1).runtime.routeActive);
        QVERIFY(!h.manager.sessionById(id)->devices.at(1).runtime.lastError.hasError());
    }

    void failedSourceStaysQuiescentUntilRetry()
    {
        ActiveHarness h;
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        const QString id = h.manager.createSession(QStringLiteral("NoSource"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        QVERIFY(h.manager.sessionById(id)->lastUsedAt.isValid() == false);
        QVERIFY(h.manager.activateSession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Failed, 2000);
        QCOMPARE(h.router.ownedLinkCount(), 0);
        QVERIFY(!h.manager.sessionById(id)->lastUsedAt.isValid());

        h.addSource();
        h.manager.refreshActiveSession();
        QVERIFY(h.manager.sessionById(id)->state == SessionState::Failed);
        QCOMPARE(h.router.ownedLinkCount(), 0);

        QVERIFY(h.manager.retrySession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
        QVERIFY(h.manager.sessionById(id)->lastUsedAt.isValid());
    }

    void staleGenerationAfterStopDoesNotRestore()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        const QString id = h.manager.createSession(QStringLiteral("Race"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        h.manager.activateSession(id);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
        h.manager.deactivateSession(id);
        QVERIFY(h.manager.sessionById(id)->state == SessionState::Idle);
        h.manager.refreshActiveSession();
        QVERIFY(h.manager.sessionById(id)->state == SessionState::Idle);
        QCOMPARE(h.router.ownedLinkCount(), 0);
    }

    void disableMemberDuringRecoveryDoesNotRestore()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, 31, 32, QStringLiteral("dest-b"), true);
        const QString id = h.manager.createSession(QStringLiteral("Disable"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:02"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        h.manager.setRecoveryPolicy(id, QStringLiteral("RestoreRoutesOnly"));
        h.manager.activateSession(id);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);

        h.endpoints.removeById(QStringLiteral("dest-b"));
        h.manager.refreshActiveSession();
        QVERIFY(h.manager.setDeviceEnabled(id, QStringLiteral("AA:BB:CC:DD:EE:02"), false)
                == SessionCommandResult::Accepted);

        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 13, 131, 132, QStringLiteral("dest-b2"), true);
        h.manager.refreshActiveSession();
        QVERIFY(h.manager.sessionById(id)->state == SessionState::Active);
        QCOMPARE(h.manager.sessionById(id)->devices.at(1).runtime.routeActive, false);
        QVERIFY(h.manager.sessionById(id)->devices.front().runtime.routeActive);
    }

    void policyNoneCancelsPendingRestore()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, 31, 32, QStringLiteral("dest-b"), true);
        const QString id = h.manager.createSession(QStringLiteral("PolicyFlip"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:02"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        h.manager.setRecoveryPolicy(id, QStringLiteral("RestoreRoutesOnly"));
        h.manager.activateSession(id);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);

        h.endpoints.removeById(QStringLiteral("dest-b"));
        h.manager.refreshActiveSession();
        QVERIFY(h.manager.setRecoveryPolicy(id, QStringLiteral("None")) == SessionCommandResult::Accepted);

        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 13, 131, 132, QStringLiteral("dest-b2"), true);
        h.manager.refreshActiveSession();
        QVERIFY(!h.manager.sessionById(id)->devices.at(1).runtime.routeActive);
    }

    void invalidVolumeRejected()
    {
        QTemporaryDir dir;
        SessionManager manager(nullptr, nullptr, dir.filePath(QStringLiteral("sessions.json")));
        manager.initialize();
        const QString id = manager.createSession(QStringLiteral("Vol"));
        QVERIFY(manager.setGroupVolume(id, std::numeric_limits<double>::quiet_NaN())
                == SessionCommandResult::InvalidArgument);
        QVERIFY(manager.setGroupVolume(id, std::numeric_limits<double>::infinity())
                == SessionCommandResult::InvalidArgument);
        QVERIFY(manager.setDeviceVolume(id, QStringLiteral("AA:BB:CC:DD:EE:01"), -std::numeric_limits<double>::infinity())
                == SessionCommandResult::InvalidArgument);
        manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        QVERIFY(manager.setDeviceVolume(id, QStringLiteral("AA:BB:CC:DD:EE:01"), -std::numeric_limits<double>::infinity())
                == SessionCommandResult::InvalidArgument);
        QVERIFY(manager.setGroupVolume(id, 1.4) == SessionCommandResult::Accepted);
        QCOMPARE(manager.sessionById(id)->groupVolume, 1.0);
        QVERIFY(manager.setGroupVolume(id, -0.2) == SessionCommandResult::Accepted);
        QCOMPARE(manager.sessionById(id)->groupVolume, 0.0);
    }

    void sourceRuntimeIdChangeStillActivates()
    {
        QTemporaryDir dir;
        const QString persist = dir.filePath(QStringLiteral("sessions.json"));
        QString id;
        {
            SessionManager manager(nullptr, nullptr, persist);
            manager.initialize();
            id = manager.createSession(QStringLiteral("StableSrc"));
            manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
            manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        }

        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        auralis::bluetooth::DeviceRegistry devices;
        FakePipeWireLinkBackend backend(&store);
        AudioRouter router(&store, &endpoints, &backend);
        SessionManager manager(&router, &endpoints, &devices, nullptr, persist);
        manager.initialize();
        router.handleConnectionState(PipeWireConnectionState::Connected, true);
        store.upsert(makeNode(99, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                   {QStringLiteral("object.serial"), QStringLiteral("7")}}));
        store.upsert(makePort(911, 99, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(912, 99, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        router.refreshSources();
        devices.upsertDevice(makeBluetoothDevice(
            QStringLiteral("/org/bluez/hci0/dev_AABBCCDDEE01"),
            QStringLiteral("AA:BB:CC:DD:EE:01"),
            QStringLiteral("dest-a"),
            true));
        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                  {QStringLiteral("node.name"), QStringLiteral("dest-a")}}));
        store.upsert(makePort(21, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(22, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        auto endpoint = makePlaybackEndpoint(QStringLiteral("dest-a"), 2, QStringLiteral("dest-a"));
        endpoint.bluetoothAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
        endpoints.upsert(endpoint);
        QVERIFY(manager.activateSession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager.sessionById(id)->state == SessionState::Active, 2000);
    }

    void persistenceFailureSurfacesError()
    {
        QTemporaryDir dir;
        const QString blocker = dir.filePath(QStringLiteral("blocker"));
        QFile file(blocker);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        SessionManager manager(nullptr, nullptr, blocker + QStringLiteral("/nested/sessions.json"));
        QSignalSpy spy(&manager, &SessionManager::sessionError);
        manager.initialize();
        const QString id = manager.createSession(QStringLiteral("PersistFail"));
        QVERIFY(id.isEmpty());
        QCOMPARE(manager.sessionCount(), 0);
        QVERIFY(spy.count() >= 1);
    }

    void midSessionSourceLossAndReturn()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        const QString id = h.manager.createSession(QStringLiteral("SrcLoss"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        QVERIFY(h.manager.activateSession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);

        h.store.remove(1);
        h.store.remove(11);
        h.store.remove(12);
        h.router.refreshSources();
        h.manager.refreshActiveSession();
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Recovering, 2000);

        h.addSource();
        h.manager.refreshActiveSession();
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
    }

    void deactivateSessionIsReentrancySafe()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, 31, 32, QStringLiteral("dest-b"), true);
        const QString id = h.manager.createSession(QStringLiteral("Stop"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:02"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        QVERIFY(h.manager.activateSession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
        const int creates = h.backend.createCalls;
        QVERIFY(h.manager.deactivateSession(id) == SessionCommandResult::Accepted);
        QVERIFY(h.manager.sessionById(id)->state == SessionState::Idle);
        QCOMPARE(h.router.ownedLinkCount(), 0);
        QCOMPARE(h.backend.createCalls, creates);
    }

    void recoveryPolicyNoneDoesNotReactivateInactiveRoute()
    {
        ActiveHarness h;
        h.addSource();
        h.addMember(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, 21, 22, QStringLiteral("dest-a"), true);
        const QString id = h.manager.createSession(QStringLiteral("NoneInactive"));
        h.manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"));
        h.manager.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
        h.manager.setRecoveryPolicy(id, QStringLiteral("None"));
        h.manager.setAutoReconnect(id, false);
        QVERIFY(h.manager.activateSession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
        const QString routeId = h.manager.sessionById(id)->devices.front().runtime.routeId;
        QVERIFY(!routeId.isEmpty());
        const int creates = h.backend.createCalls;
        h.router.deactivateRoute(routeId);
        h.manager.refreshActiveSession();
        QVERIFY(h.manager.sessionById(id)->state == SessionState::Degraded
            || h.manager.sessionById(id)->state == SessionState::Failed);
        QCOMPARE(h.backend.createCalls, creates);
        const auto route = h.router.routeById(routeId);
        QVERIFY(route.has_value());
        QVERIFY(route->state != auralis::audio::RouteState::Active);

        QVERIFY(h.manager.retrySession(id) == SessionCommandResult::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(h.manager.sessionById(id)->state == SessionState::Active, 2000);
    }
};

QTEST_GUILESS_MAIN(TstSessionManager)
#include "tst_SessionManager.moc"

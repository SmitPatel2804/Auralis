#include "../unit/audio/AudioTestFixtures.h"
#include "../unit/audio/FakePipeWireLinkBackend.h"
#include "../unit/bluetooth/FakeBlueZClient.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/bluetooth/BlueZConstants.h>
#include <auralis/bluetooth/ReconnectPolicy.h>
#include <auralis/session/SessionManager.h>
#include <auralis/session/SessionTypes.h>

Q_DECLARE_METATYPE(auralis::session::SessionError)

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using auralis::audio::AudioEndpointRegistry;
using auralis::audio::AudioRouter;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireObjectStore;
using auralis::bluetooth::BluetoothManager;
using auralis::bluetooth::ReconnectPolicyConfig;
using auralis::session::SessionCommandResult;
using auralis::session::SessionError;
using auralis::session::SessionManager;
using auralis::session::SessionState;
using auralis::test::FakeBlueZClient;
using auralis::test::FakePipeWireLinkBackend;
using auralis::test::makeNode;
using auralis::test::makePlaybackEndpoint;
using auralis::test::makePort;

namespace {

QVariantMap poweredAdapter()
{
    return {
        {QStringLiteral("Address"), QStringLiteral("E8:9E:B4:13:4C:CC")},
        {QStringLiteral("Alias"), QStringLiteral("smit")},
        {QStringLiteral("Powered"), true},
        {QStringLiteral("Discovering"), false},
    };
}

QVariantMap pairedDevice(const QString& address, bool connected)
{
    return {
        {QStringLiteral("Address"), address},
        {QStringLiteral("Alias"), address},
        {QStringLiteral("AddressType"), QStringLiteral("public")},
        {QStringLiteral("Adapter"), QStringLiteral("/org/bluez/hci0")},
        {QStringLiteral("Paired"), true},
        {QStringLiteral("Connected"), connected},
    };
}

QString devicePath(const QString& address)
{
    return QStringLiteral("/org/bluez/hci0/dev_%1").arg(QString(address).remove(QLatin1Char(':')));
}

} // namespace

class TstSessionManagedReconnectIntegration : public QObject {
    Q_OBJECT

private:
    struct Stack {
        QTemporaryDir tempDir;
        FakeBlueZClient* client = new FakeBlueZClient;
        BluetoothManager bluetooth{client};
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        FakePipeWireLinkBackend backend{&store};
        AudioRouter router{&store, &endpoints, &backend};
        SessionManager sessions;

        Stack()
            : sessions(
                  &router,
                  &endpoints,
                  bluetooth.deviceRegistry(),
                  &bluetooth,
                  tempDir.filePath(QStringLiteral("sessions.json")))
        {
            client->setAdapter(QStringLiteral("/org/bluez/hci0"), poweredAdapter());
            client->setDevice(devicePath(QStringLiteral("AA:BB:CC:DD:EE:01")), pairedDevice(QStringLiteral("AA:BB:CC:DD:EE:01"), true));
            client->setDevice(devicePath(QStringLiteral("AA:BB:CC:DD:EE:02")), pairedDevice(QStringLiteral("AA:BB:CC:DD:EE:02"), true));
            ReconnectPolicyConfig config;
            config.initialDelayMs = 400;
            config.maxDelayMs = 400;
            config.maxAttempts = 5;
            config.backoffMultiplier = 1.0;
            bluetooth.setReconnectPolicyConfig(config);
            bluetooth.initialize();
            router.handleConnectionState(PipeWireConnectionState::Connected, true);
            sessions.initialize();
            addSource();
            addEndpoint(QStringLiteral("AA:BB:CC:DD:EE:01"), 2, QStringLiteral("dest-a"));
            addEndpoint(QStringLiteral("AA:BB:CC:DD:EE:02"), 3, QStringLiteral("dest-b"));
        }

        void addSource()
        {
            store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                      {QStringLiteral("object.serial"), QStringLiteral("7")}}));
            store.upsert(makePort(11, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
            store.upsert(makePort(12, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
            router.refreshSources();
        }

        void addEndpoint(const QString& address, quint32 nodeId, const QString& endpointId)
        {
            store.upsert(makeNode(nodeId, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                           {QStringLiteral("node.name"), endpointId}}));
            store.upsert(makePort(nodeId * 10 + 1, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
            store.upsert(makePort(nodeId * 10 + 2, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
            auto endpoint = makePlaybackEndpoint(endpointId, nodeId, endpointId);
            endpoint.bluetoothAddress = address;
            endpoints.upsert(endpoint);
        }

        QString startActiveSession()
        {
            const QString id = sessions.createSession(QStringLiteral("Pair"));
            sessions.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("Left"));
            sessions.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("Right"));
            sessions.setSource(id, QStringLiteral("src:7:Stream/Output/Audio"));
            if (sessions.activateSession(id) != SessionCommandResult::Accepted) {
                return {};
            }
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 2000) {
                const auto session = sessions.sessionById(id);
                if (session.has_value() && session->state == SessionState::Active) {
                    return id;
                }
                QTest::qWait(20);
            }
            return {};
        }

        void disconnectMember(const QString& address, const QString& endpointId)
        {
            endpoints.removeById(endpointId);
            client->updateDevice(devicePath(address), {{QStringLiteral("Connected"), false}}, {});
            sessions.refreshActiveSession();
        }
    };

private slots:
    void oneDisconnectSchedulesReconnectOnce()
    {
        Stack stack;
        const QString id = stack.startActiveSession();
        QVERIFY(stack.sessions.sessionById(id).has_value());
        QVERIFY(stack.sessions.sessionById(id)->state == SessionState::Active);
        QSignalSpy dueSpy(stack.bluetooth.reconnectPolicy(), &auralis::bluetooth::ReconnectPolicy::reconnectDue);

        stack.disconnectMember(QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("dest-b"));
        stack.client->updateDevice(
            devicePath(QStringLiteral("AA:BB:CC:DD:EE:02")), {{QStringLiteral("Connected"), false}}, {});
        stack.client->updateDevice(
            devicePath(QStringLiteral("AA:BB:CC:DD:EE:02")), {{QStringLiteral("Connected"), false}}, {});

        QCOMPARE(stack.bluetooth.reconnectPolicy()->attempt(devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"))), 1);
        QVERIFY(stack.bluetooth.reconnectPolicy()->isScheduled(devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"))));
        QCOMPARE(dueSpy.count(), 0);
        QVERIFY(stack.sessions.sessionById(id)->devices.front().runtime.routeActive);
    }

    void reconnectExhaustionLeavesPeerHealthyDegraded()
    {
        Stack stack;
        ReconnectPolicyConfig config;
        config.initialDelayMs = 30;
        config.maxDelayMs = 30;
        config.maxAttempts = 2;
        config.backoffMultiplier = 1.0;
        stack.bluetooth.setReconnectPolicyConfig(config);
        stack.client->setConnectResult(
            false,
            auralis::bluetooth::bluez::kErrorConnectionAttemptFailed.toString(),
            QStringLiteral("failed"));

        const QString id = stack.startActiveSession();
        QVERIFY(stack.sessions.sessionById(id).has_value());
        QVERIFY(stack.sessions.sessionById(id)->state == SessionState::Active);
        QSignalSpy errorSpy(&stack.sessions, &SessionManager::sessionError);
        QSignalSpy exhaustedSpy(&stack.bluetooth, &BluetoothManager::managedReconnectExhausted);

        stack.disconnectMember(QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("dest-b"));
        QTRY_VERIFY_WITH_TIMEOUT(exhaustedSpy.count() >= 1, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(errorSpy.count() >= 1, 2000);
        bool sawExhausted = false;
        for (int i = 0; i < errorSpy.count(); ++i) {
            if (errorSpy.at(i).at(1).value<SessionError>() == SessionError::RecoveryExhausted) {
                sawExhausted = true;
                break;
            }
        }
        QVERIFY(sawExhausted);
        QVERIFY(stack.sessions.sessionById(id)->state != SessionState::Recovering);
        QVERIFY(stack.sessions.sessionById(id)->state == SessionState::Degraded);
        QVERIFY(stack.sessions.sessionById(id)->devices.front().runtime.routeActive);
        QVERIFY(!stack.sessions.sessionById(id)->devices.at(1).runtime.recovering);
    }

    void reconnectExhaustionWithNoHealthyMembersFails()
    {
        Stack stack;
        ReconnectPolicyConfig config;
        config.initialDelayMs = 30;
        config.maxDelayMs = 30;
        config.maxAttempts = 1;
        config.backoffMultiplier = 1.0;
        stack.bluetooth.setReconnectPolicyConfig(config);
        stack.client->setConnectResult(
            false,
            auralis::bluetooth::bluez::kErrorConnectionAttemptFailed.toString(),
            QStringLiteral("failed"));

        const QString id = stack.startActiveSession();
        QVERIFY(stack.sessions.sessionById(id).has_value());
        QVERIFY(stack.sessions.sessionById(id)->state == SessionState::Active);
        stack.disconnectMember(QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("dest-a"));
        stack.disconnectMember(QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("dest-b"));
        QTRY_VERIFY_WITH_TIMEOUT(stack.sessions.sessionById(id)->state == SessionState::Failed, 3000);
    }

    void policyNoneCancelsLowerLevelReconnect()
    {
        Stack stack;
        const QString id = stack.startActiveSession();
        QVERIFY(stack.sessions.sessionById(id).has_value());
        QVERIFY(stack.sessions.sessionById(id)->state == SessionState::Active);
        stack.disconnectMember(QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("dest-b"));
        QVERIFY(stack.bluetooth.reconnectPolicy()->isScheduled(devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"))));
        QVERIFY(stack.sessions.setRecoveryPolicy(id, QStringLiteral("None")) == SessionCommandResult::Accepted);
        QVERIFY(!stack.bluetooth.reconnectPolicy()->isScheduled(devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"))));
        QVERIFY(!stack.bluetooth.reconnectPolicy()->isReconnectInProgress(devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"))));
    }

    void activeSessionSuppressesLowerLayerAutoReconnect()
    {
        Stack stack;
        const QString id = stack.startActiveSession();
        QVERIFY(stack.sessions.sessionById(id).has_value());
        QVERIFY(stack.sessions.sessionById(id)->state == SessionState::Active);

        stack.sessions.setRecoveryPolicy(id, QStringLiteral("None"));

        stack.endpoints.removeById(QStringLiteral("dest-b"));
        stack.client->updateDevice(devicePath(QStringLiteral("AA:BB:CC:DD:EE:02")), {{QStringLiteral("Connected"), false}}, {});
        stack.sessions.refreshActiveSession();

        QTest::qWait(100);
        QVERIFY(!stack.bluetooth.reconnectPolicy()->isScheduled(devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"))));

        stack.sessions.deactivateSession(id);
        stack.client->updateDevice(devicePath(QStringLiteral("AA:BB:CC:DD:EE:02")), {{QStringLiteral("Connected"), false}}, {});
        QTest::qWait(100);
    }

    void terminalReconnectFailureReachesSessionState()
    {
        Stack stack;
        stack.client->setConnectResult(
            false,
            QStringLiteral("org.bluez.Error.AuthenticationFailed"),
            QStringLiteral("auth failed"));

        const QString id = stack.startActiveSession();
        QVERIFY(stack.sessions.sessionById(id).has_value());
        QVERIFY(stack.sessions.sessionById(id)->state == SessionState::Active);

        QSignalSpy errorSpy(&stack.sessions, &SessionManager::sessionError);

        stack.disconnectMember(QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("dest-b"));
        QTRY_VERIFY_WITH_TIMEOUT(errorSpy.count() >= 1, 2000);
        bool sawExhausted = false;
        for (int i = 0; i < errorSpy.count(); ++i) {
            if (errorSpy.at(i).at(1).value<SessionError>() == SessionError::RecoveryExhausted) {
                sawExhausted = true;
                break;
            }
        }
        QVERIFY(sawExhausted);
        QVERIFY(!stack.sessions.sessionById(id)->devices.at(1).runtime.recovering);
        QVERIFY(stack.sessions.sessionById(id)->devices.front().runtime.routeActive);
    }
    void devicePreferenceBlocksReconnect()
    {
        Stack stack;
        const QString id = stack.startActiveSession();
        QVERIFY(!id.isEmpty());

        // Ensure policy allows reconnect
        stack.sessions.setAutoReconnect(id, true);
        stack.sessions.setRecoveryPolicy(id, QStringLiteral("ReconnectAndRestore"));

        // Set device B's autoReconnectEnabled=false
        const QString pathB = devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"));
        stack.bluetooth.deviceRegistry()->setAutoReconnectEnabled(pathB, false);

        // Disconnect member B
        stack.disconnectMember(QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("dest-b"));

        QTest::qWait(100);
        // Reconnect should NOT be scheduled for B (device preference blocks it)
        QVERIFY(!stack.bluetooth.reconnectPolicy()->isScheduled(pathB));
    }

    void devicePreferenceAllowsReconnect()
    {
        Stack stack;
        const QString id = stack.startActiveSession();
        QVERIFY(!id.isEmpty());

        stack.sessions.setAutoReconnect(id, true);
        stack.sessions.setRecoveryPolicy(id, QStringLiteral("ReconnectAndRestore"));

        // Device B has autoReconnectEnabled=true (default)
        const QString pathB = devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"));

        stack.disconnectMember(QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("dest-b"));

        // Reconnect SHOULD be scheduled for B
        QVERIFY(stack.bluetooth.reconnectPolicy()->isScheduled(pathB));
    }

    void sessionPolicyNoneBlocksReconnect()
    {
        Stack stack;
        const QString id = stack.startActiveSession();
        QVERIFY(!id.isEmpty());

        stack.sessions.setAutoReconnect(id, true);
        stack.sessions.setRecoveryPolicy(id, QStringLiteral("None"));

        const QString pathB = devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"));
        stack.disconnectMember(QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("dest-b"));

        QTest::qWait(100);
        QVERIFY(!stack.bluetooth.reconnectPolicy()->isScheduled(pathB));
    }

    void devicePreferenceSurvivesSessionLifecycle()
    {
        Stack stack;
        const QString id = stack.startActiveSession();
        QVERIFY(!id.isEmpty());

        const QString pathB = devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"));
        stack.bluetooth.deviceRegistry()->setAutoReconnectEnabled(pathB, false);

        stack.sessions.deactivateSession(id);

        // Device preference must be preserved
        const auto* dev = stack.bluetooth.deviceRegistry()->findByObjectPath(pathB);
        QVERIFY(dev != nullptr);
        QVERIFY(!dev->autoReconnectEnabled);
    }

    void disableRemovesSuppression()
    {
        Stack stack;
        const QString id = stack.startActiveSession();
        QVERIFY(!id.isEmpty());
        const QString pathB = devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"));

        // Member B should be suppressed in an active session
        QVERIFY(stack.bluetooth.isAutoReconnectSuppressed(pathB));

        // Disable member B
        stack.sessions.setDeviceEnabled(id, QStringLiteral("AA:BB:CC:DD:EE:02"), false);

        // Suppression removed for disabled member
        QVERIFY(!stack.bluetooth.isAutoReconnectSuppressed(pathB));
    }

    void enableReappliesSuppression()
    {
        Stack stack;
        const QString id = stack.startActiveSession();
        QVERIFY(!id.isEmpty());
        const QString pathB = devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"));

        stack.sessions.setDeviceEnabled(id, QStringLiteral("AA:BB:CC:DD:EE:02"), false);
        QVERIFY(!stack.bluetooth.isAutoReconnectSuppressed(pathB));

        // Re-enable member B
        stack.sessions.setDeviceEnabled(id, QStringLiteral("AA:BB:CC:DD:EE:02"), true);
        QVERIFY(stack.bluetooth.isAutoReconnectSuppressed(pathB));
    }

    void stopRestoresDeviceBehavior()
    {
        Stack stack;
        const QString id = stack.startActiveSession();
        QVERIFY(!id.isEmpty());
        const QString pathA = devicePath(QStringLiteral("AA:BB:CC:DD:EE:01"));
        const QString pathB = devicePath(QStringLiteral("AA:BB:CC:DD:EE:02"));

        QVERIFY(stack.bluetooth.isAutoReconnectSuppressed(pathA));
        QVERIFY(stack.bluetooth.isAutoReconnectSuppressed(pathB));

        stack.sessions.deactivateSession(id);
        QVERIFY(!stack.bluetooth.isAutoReconnectSuppressed(pathA));
        QVERIFY(!stack.bluetooth.isAutoReconnectSuppressed(pathB));
    }
};

QTEST_GUILESS_MAIN(TstSessionManagedReconnectIntegration)
#include "tst_SessionManagedReconnectIntegration.moc"

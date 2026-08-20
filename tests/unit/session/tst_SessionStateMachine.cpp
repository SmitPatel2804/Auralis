#include <auralis/session/AuralisSession.h>
#include <auralis/session/SessionStateMachine.h>

#include <QtTest>

using auralis::session::SessionError;
using auralis::session::SessionHealthSnapshot;
using auralis::session::SessionIntent;
using auralis::session::SessionState;
using auralis::session::SessionStateMachine;

class TstSessionStateMachine : public QObject {
    Q_OBJECT

private slots:
    void idleStaysIdleWithoutIntent()
    {
        SessionHealthSnapshot health;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Idle, SessionIntent::None, health) == SessionState::Idle);
    }

    void startingBecomesActiveWhenAllMembersHealthy()
    {
        SessionHealthSnapshot health;
        health.sourceAvailable = true;
        health.enabledCount = 2;
        health.routeActiveCount = 2;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Starting, SessionIntent::Starting, health)
            == SessionState::Active);
    }

    void startingBecomesDegradedWithPartialRoutes()
    {
        SessionHealthSnapshot health;
        health.sourceAvailable = true;
        health.enabledCount = 2;
        health.routeActiveCount = 1;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Starting, SessionIntent::Starting, health)
            == SessionState::Degraded);
    }

    void startingBecomesFailedWithNoRoutes()
    {
        SessionHealthSnapshot health;
        health.sourceAvailable = true;
        health.enabledCount = 2;
        health.terminalFailure = true;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Starting, SessionIntent::Starting, health)
            == SessionState::Failed);
    }

    void startingStaysStartingWhileRoutesPending()
    {
        SessionHealthSnapshot health;
        health.sourceAvailable = true;
        health.enabledCount = 2;
        health.pendingCount = 2;
        health.terminalFailure = false;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Starting, SessionIntent::Starting, health)
            == SessionState::Starting);
    }

    void startingWithoutPendingDoesNotFailUnlessTerminal()
    {
        SessionHealthSnapshot health;
        health.sourceAvailable = true;
        health.enabledCount = 1;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Starting, SessionIntent::Starting, health)
            == SessionState::Starting);
    }

    void activeBecomesDegradedWhenMemberLost()
    {
        SessionHealthSnapshot health;
        health.sourceAvailable = true;
        health.enabledCount = 2;
        health.routeActiveCount = 1;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Active, SessionIntent::None, health)
            == SessionState::Degraded);
    }

    void recoveringReturnsActiveWhenAllHealthy()
    {
        SessionHealthSnapshot health;
        health.sourceAvailable = true;
        health.enabledCount = 2;
        health.routeActiveCount = 2;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Recovering, SessionIntent::Recovering, health)
            == SessionState::Active);
    }

    void recoveringWithNoRoutesFailsWhenNothingRecovering()
    {
        SessionHealthSnapshot health;
        health.sourceAvailable = true;
        health.enabledCount = 2;
        health.pendingCount = 2;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Recovering, SessionIntent::Recovering, health)
            == SessionState::Failed);
    }

    void leftoverRouteIdIsNotPendingHealth()
    {
        auralis::session::AuralisSession session;
        session.state = SessionState::Recovering;
        session.sourceId = QStringLiteral("src:1");
        auralis::session::SessionDevice device;
        device.enabled = true;
        device.runtime.routeId = QStringLiteral("stale");
        device.runtime.routeRequested = false;
        device.runtime.routeActive = false;
        session.devices.push_back(device);
        const auralis::session::SessionHealthSnapshot health =
            auralis::session::healthSnapshotFromSession(session, true);
        QCOMPARE(health.pendingCount, 0);
        QVERIFY(health.terminalFailure);
    }

    void stoppingBecomesIdleWhenRoutesGone()
    {
        SessionHealthSnapshot health;
        health.enabledCount = 2;
        health.routeActiveCount = 0;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Stopping, SessionIntent::Stopping, health)
            == SessionState::Idle);
    }

    void failedStaysFailedWithoutStartingIntent()
    {
        SessionHealthSnapshot health;
        health.sourceAvailable = true;
        health.enabledCount = 1;
        health.routeActiveCount = 0;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Failed, SessionIntent::None, health)
            == SessionState::Failed);
    }

    void failedCanRestart()
    {
        SessionHealthSnapshot health;
        health.sourceAvailable = true;
        health.enabledCount = 1;
        health.routeActiveCount = 0;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Failed, SessionIntent::Starting, health)
            == SessionState::Starting);
    }

    void startHealthIsNotTerminalWhileRoutePending()
    {
        auralis::session::AuralisSession session;
        session.state = SessionState::Starting;
        session.sourceId = QStringLiteral("src:1");
        auralis::session::SessionDevice device;
        device.enabled = true;
        device.runtime.routeRequested = true;
        session.devices.push_back(device);
        const auralis::session::SessionHealthSnapshot health =
            auralis::session::healthSnapshotFromSession(session, true);
        QVERIFY(!health.terminalFailure);
        QCOMPARE(health.pendingCount, 1);
        QCOMPARE(health.routeActiveCount, 0);
    }

    void startHealthIsTerminalWhenSourceMissing()
    {
        auralis::session::AuralisSession session;
        session.state = SessionState::Starting;
        session.sourceId = QStringLiteral("src:missing");
        auralis::session::SessionDevice device;
        device.enabled = true;
        session.devices.push_back(device);
        const auralis::session::SessionHealthSnapshot health =
            auralis::session::healthSnapshotFromSession(session, false);
        QVERIFY(health.terminalFailure);
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Starting, SessionIntent::Starting, health)
            == SessionState::Failed);
    }

    void startHealthIsTerminalWhenRouteFailed()
    {
        auralis::session::AuralisSession session;
        session.state = SessionState::Starting;
        session.sourceId = QStringLiteral("src:1");
        auralis::session::SessionDevice device;
        device.enabled = true;
        device.runtime.routeRequested = false;
        device.runtime.routeActive = false;
        device.runtime.lastError = {
            SessionError::RouteActivationFailed,
            QStringLiteral("Activation timed out")};
        session.devices.push_back(device);
        const auralis::session::SessionHealthSnapshot health =
            auralis::session::healthSnapshotFromSession(session, true);
        QVERIFY(health.terminalFailure);
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Starting, SessionIntent::Starting, health)
            == SessionState::Failed);
    }
};

QTEST_GUILESS_MAIN(TstSessionStateMachine)
#include "tst_SessionStateMachine.moc"

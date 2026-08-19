#include <auralis/session/SessionStateMachine.h>

#include <QtTest>

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

    void stoppingBecomesIdleWhenRoutesGone()
    {
        SessionHealthSnapshot health;
        health.enabledCount = 2;
        health.routeActiveCount = 0;
        QVERIFY(
            SessionStateMachine::recompute(SessionState::Stopping, SessionIntent::Stopping, health)
            == SessionState::Idle);
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
};

QTEST_GUILESS_MAIN(TstSessionStateMachine)
#include "tst_SessionStateMachine.moc"

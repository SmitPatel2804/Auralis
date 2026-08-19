#include <auralis/bluetooth/ReconnectPolicy.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::ReconnectPolicy;
using auralis::bluetooth::ReconnectPolicyConfig;

class TstReconnectPolicy : public QObject {
    Q_OBJECT

private slots:
    void emitsWithBackoffAndMaxAttempts()
    {
        ReconnectPolicy policy;
        ReconnectPolicyConfig config;
        config.enabled = true;
        config.maxAttempts = 3;
        config.initialDelayMs = 50;
        config.maxDelayMs = 200;
        config.backoffMultiplier = 2.0;
        policy.setConfig(config);

        QSignalSpy dueSpy(&policy, &ReconnectPolicy::reconnectDue);
        policy.scheduleReconnect(QStringLiteral("/org/bluez/hci0/dev_AA"));
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);
        QCOMPARE(dueSpy.at(0).at(1).toInt(), 1);
        QCOMPARE(dueSpy.at(0).at(2).toInt(), 3);
        policy.completeReconnectAttempt(QStringLiteral("/org/bluez/hci0/dev_AA"));

        policy.scheduleReconnect(QStringLiteral("/org/bluez/hci0/dev_AA"));
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 2, 500);
        QCOMPARE(dueSpy.at(1).at(1).toInt(), 2);
        policy.completeReconnectAttempt(QStringLiteral("/org/bluez/hci0/dev_AA"));

        policy.scheduleReconnect(QStringLiteral("/org/bluez/hci0/dev_AA"));
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 3, 500);
        policy.completeReconnectAttempt(QStringLiteral("/org/bluez/hci0/dev_AA"));

        policy.scheduleReconnect(QStringLiteral("/org/bluez/hci0/dev_AA"));
        QTest::qWait(300);
        QCOMPARE(dueSpy.count(), 3);
    }

    void cancelStopsPendingReconnect()
    {
        ReconnectPolicy policy;
        ReconnectPolicyConfig config;
        config.initialDelayMs = 500;
        policy.setConfig(config);

        QSignalSpy dueSpy(&policy, &ReconnectPolicy::reconnectDue);
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        QVERIFY(policy.isScheduled(QStringLiteral("/dev/1")));
        policy.cancelReconnect(QStringLiteral("/dev/1"));
        QVERIFY(!policy.isScheduled(QStringLiteral("/dev/1")));
        QTest::qWait(600);
        QCOMPARE(dueSpy.count(), 0);
    }

    void pauseAllClearsScheduled()
    {
        ReconnectPolicy policy;
        ReconnectPolicyConfig config;
        config.initialDelayMs = 500;
        policy.setConfig(config);
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        policy.pauseAll();
        QVERIFY(!policy.isScheduled(QStringLiteral("/dev/1")));
    }

    void resumeAllRestoresScheduling()
    {
        ReconnectPolicy policy;
        ReconnectPolicyConfig config;
        config.initialDelayMs = 50;
        policy.setConfig(config);

        QSignalSpy dueSpy(&policy, &ReconnectPolicy::reconnectDue);
        policy.pauseAll();
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        QTest::qWait(100);
        QCOMPARE(dueSpy.count(), 0);

        policy.resumeAll();
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);
    }

    void onConnectedClearsAttempts()
    {
        ReconnectPolicy policy;
        ReconnectPolicyConfig config;
        config.initialDelayMs = 50;
        policy.setConfig(config);
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        QTRY_VERIFY_WITH_TIMEOUT(policy.attempt(QStringLiteral("/dev/1")) >= 1, 500);
        policy.onConnected(QStringLiteral("/dev/1"));
        QCOMPARE(policy.attempt(QStringLiteral("/dev/1")), 0);
    }

    void duplicateScheduleWhileTimerActiveDoesNotConsumeAttempt()
    {
        ReconnectPolicy policy;
        ReconnectPolicyConfig config;
        config.initialDelayMs = 400;
        config.maxAttempts = 5;
        policy.setConfig(config);
        QSignalSpy dueSpy(&policy, &ReconnectPolicy::reconnectDue);
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        QCOMPARE(policy.attempt(QStringLiteral("/dev/1")), 1);
        QVERIFY(policy.isScheduled(QStringLiteral("/dev/1")));
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        QCOMPARE(policy.attempt(QStringLiteral("/dev/1")), 1);
        QCOMPARE(dueSpy.count(), 0);
        QVERIFY(policy.isScheduled(QStringLiteral("/dev/1")));
    }

    void emitsExhaustedOnceAfterBudget()
    {
        ReconnectPolicy policy;
        ReconnectPolicyConfig config;
        config.enabled = true;
        config.maxAttempts = 1;
        config.initialDelayMs = 30;
        config.maxDelayMs = 30;
        policy.setConfig(config);

        QSignalSpy dueSpy(&policy, &ReconnectPolicy::reconnectDue);
        QSignalSpy exhaustedSpy(&policy, &ReconnectPolicy::reconnectExhausted);
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        QTRY_COMPARE_WITH_TIMEOUT(dueSpy.count(), 1, 500);
        policy.completeReconnectAttempt(QStringLiteral("/dev/1"));
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        QCOMPARE(exhaustedSpy.count(), 1);
        QCOMPARE(exhaustedSpy.at(0).at(1).toInt(), 1);
        policy.scheduleReconnect(QStringLiteral("/dev/1"));
        QCOMPARE(exhaustedSpy.count(), 1);
        QCOMPARE(dueSpy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TstReconnectPolicy)
#include "tst_ReconnectPolicy.moc"

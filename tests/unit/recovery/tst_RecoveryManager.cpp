#include <auralis/recovery/RecoveryManager.h>
#include <auralis/recovery/ServiceRetryPolicy.h>
#include <auralis/recovery/SystemPowerMonitor.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::recovery::RecoveryManager;
using auralis::recovery::RecoveryState;
using auralis::recovery::ServiceRetryPolicy;
using auralis::recovery::ServiceRetryPolicyConfig;
using auralis::recovery::SystemPowerMonitor;

class TstRecoveryManager : public QObject {
    Q_OBJECT

private slots:
    void retryPolicyBackoffIsBounded()
    {
        ServiceRetryPolicy policy({.maxAttempts = 4, .initialDelayMs = 100, .maxDelayMs = 250, .backoffMultiplier = 2.0});
        QCOMPARE(policy.consumeAttemptDelayMs(), 100);
        QCOMPARE(policy.consumeAttemptDelayMs(), 200);
        QCOMPARE(policy.consumeAttemptDelayMs(), 250);
        QCOMPARE(policy.consumeAttemptDelayMs(), 250);
        QVERIFY(policy.exhausted());
        QVERIFY(!policy.canAttempt());
    }

    void blueZBouncePausesAndResumesReconnect()
    {
        RecoveryManager manager;
        int pauseCount = 0;
        int resumeCount = 0;
        int refreshCount = 0;
        int sessionRefresh = 0;

        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { ++pauseCount; };
        hooks.resumeBluetoothReconnect = [&]() { ++resumeCount; };
        hooks.requestBlueZRefresh = [&]() { ++refreshCount; };
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());

        manager.notifyBlueZAvailable(false);
        QCOMPARE(pauseCount, 1);
        QVERIFY(manager.status().bluetooth == RecoveryState::Waiting);

        manager.notifyBlueZAvailable(true);
        QCOMPARE(resumeCount, 1);
        QCOMPARE(refreshCount, 1);
        manager.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 1);
        QVERIFY(manager.status().overall == RecoveryState::Healthy);
    }

    void pipeWireErrorMarksRecoveringIdempotently()
    {
        RecoveryManager manager;
        RecoveryManager::HostHooks hooks;
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return false; };
        hooks.isPipeWireGraphReady = []() { return false; };
        hooks.isSystemBusConnected = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());

        manager.notifyPipeWireError(QStringLiteral("boom"));
        manager.notifyPipeWireError(QStringLiteral("boom2"));
        QVERIFY(manager.status().pipeWire == RecoveryState::Recovering);
        QVERIFY(manager.status().overall == RecoveryState::Recovering);

        manager.notifyPipeWireConnected(true, true);
        manager.flushPendingReconcileForTesting();
        QVERIFY(manager.status().pipeWire == RecoveryState::Healthy);
        QVERIFY(manager.status().overall == RecoveryState::Healthy);
    }

    void suspendResumeBumpsEpochAndReconciles()
    {
        RecoveryManager manager;
        int sessionRefresh = 0;
        int pauseCount = 0;
        int resumeCount = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { ++pauseCount; };
        hooks.resumeBluetoothReconnect = [&]() { ++resumeCount; };
        hooks.requestBlueZRefresh = []() {};
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());

        const quint64 genBefore = manager.generation();
        manager.onPreparingForSleep(true);
        QVERIFY(manager.suspended());
        QVERIFY(manager.status().overall == RecoveryState::Suspended);
        QCOMPARE(pauseCount, 1);
        QVERIFY(manager.generation() > genBefore);

        const quint64 epochBefore = manager.suspendEpoch();
        manager.onPreparingForSleep(false);
        QVERIFY(!manager.suspended());
        QVERIFY(manager.suspendEpoch() > epochBefore);
        QCOMPARE(resumeCount, 1);
        manager.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 1);
    }

    void shutdownIgnoresFurtherEvents()
    {
        RecoveryManager manager;
        int pauseCount = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { ++pauseCount; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        manager.shutdown();
        manager.notifyBlueZAvailable(false);
        QCOMPARE(pauseCount, 0);
    }

    void powerMonitorInjectDoesNotRequireLogind()
    {
        SystemPowerMonitor monitor;
        QVERIFY(monitor.initialize());
        QSignalSpy spy(&monitor, &SystemPowerMonitor::preparingForSleep);
        monitor.injectPrepareForSleep(true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(monitor.suspended());
        monitor.injectPrepareForSleep(false);
        QCOMPARE(spy.count(), 2);
        QVERIFY(!monitor.suspended());
        monitor.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstRecoveryManager)
#include "tst_RecoveryManager.moc"

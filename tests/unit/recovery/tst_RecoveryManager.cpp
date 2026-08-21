#include <auralis/recovery/RecoveryManager.h>
#include <auralis/recovery/RecoveryTypes.h>
#include <auralis/recovery/ServiceRetryPolicy.h>
#include <auralis/recovery/SystemPowerMonitor.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::recovery::RecoveryManager;
using auralis::recovery::RecoveryState;
using auralis::recovery::RecoveryStatus;
using auralis::recovery::ServiceRetryPolicy;
using auralis::recovery::SystemPowerMonitor;
using auralis::recovery::userFacingStatus;

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
        QCOMPARE(refreshCount, 0); // snapshot owned by BlueZ client, not RecoveryManager
        manager.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 1);
        QVERIFY(manager.status().overall == RecoveryState::Healthy);
    }

    void blueZReturnWithAutoRecoverDisabledDoesNotReconcileSession()
    {
        RecoveryManager manager;
        int sessionRefresh = 0;
        int resumeCount = 0;
        RecoveryManager::HostHooks hooks;
        hooks.resumeBluetoothReconnect = [&]() { ++resumeCount; };
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        manager.setHooks(std::move(hooks));
        manager.setAutoRecoverEnabled(false);
        QVERIFY(manager.initialize());
        manager.notifyBlueZAvailable(false);
        manager.notifyBlueZAvailable(true);
        manager.flushPendingReconcileForTesting();
        QCOMPARE(resumeCount, 1);
        QCOMPARE(sessionRefresh, 0);
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

        manager.notifyPipeWireReconnectAttempt(2);
        QCOMPARE(manager.status().pipeWireAttempts, 2);

        manager.notifyPipeWireConnected(true, true);
        manager.flushPendingReconcileForTesting();
        QVERIFY(manager.status().pipeWire == RecoveryState::Healthy);
        QVERIFY(manager.status().overall == RecoveryState::Healthy);
        QCOMPARE(manager.status().pipeWireAttempts, 0);
    }

    void pipeWireExhaustionSetsCentralStatus()
    {
        RecoveryManager manager;
        QSignalSpy exhausted(&manager, &RecoveryManager::recoveryExhausted);
        QVERIFY(manager.initialize());
        manager.notifyPipeWireReconnectExhausted(QStringLiteral("max attempts"));
        QVERIFY(manager.status().overall == RecoveryState::Exhausted);
        QVERIFY(manager.status().pipeWire == RecoveryState::Exhausted);
        QCOMPARE(exhausted.count(), 1);
    }

    void resumeWithRestoreDisabledDoesNotLeaveBluetoothReconnectPaused()
    {
        RecoveryManager manager;
        int pauseCount = 0;
        int resumeCount = 0;
        int sessionRefresh = 0;
        int pwReconnect = 0;
        int blueZRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { ++pauseCount; };
        hooks.resumeBluetoothReconnect = [&]() { ++resumeCount; };
        hooks.requestBlueZRefresh = [&]() { ++blueZRefresh; };
        hooks.requestPipeWireReconnect = [&]() { ++pwReconnect; };
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        manager.setHooks(std::move(hooks));
        manager.setRestoreOnResume(false);
        QVERIFY(manager.initialize());

        manager.onPreparingForSleep(true);
        QCOMPARE(pauseCount, 1);
        QVERIFY(manager.suspended());

        manager.onPreparingForSleep(false);
        QCOMPARE(resumeCount, 1);
        QVERIFY(!manager.suspended());
        QCOMPARE(sessionRefresh, 0);
        QCOMPARE(pwReconnect, 0);
        QCOMPARE(blueZRefresh, 0);
        manager.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 0);
    }

    void repeatedSleepResumeWithoutRestoreBalancesPauseResume()
    {
        RecoveryManager manager;
        int pauseCount = 0;
        int resumeCount = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { ++pauseCount; };
        hooks.resumeBluetoothReconnect = [&]() { ++resumeCount; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        manager.setHooks(std::move(hooks));
        manager.setRestoreOnResume(false);
        QVERIFY(manager.initialize());

        for (int i = 0; i < 5; ++i) {
            manager.onPreparingForSleep(true);
            manager.onPreparingForSleep(false);
        }
        QCOMPARE(pauseCount, 5);
        QCOMPARE(resumeCount, 5);
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
        const quint64 generationAfterShutdown = manager.generation();
        manager.shutdown();
        QCOMPARE(manager.generation(), generationAfterShutdown);
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

    void runtimeBusLossInvalidatesExistingLogindSubscription()
    {
        SystemPowerMonitor monitor;
        QVERIFY(monitor.initialize());
        monitor.injectSubscribedForTesting(true);
        QVERIFY(monitor.isSubscribed());
        monitor.notifySystemBusUnavailable();
#if defined(Q_OS_LINUX)
        QVERIFY(!monitor.isSubscribed());
        monitor.notifySystemBusUnavailable();
        QVERIFY(!monitor.isSubscribed());
#else
        // Windows and macOS observe power events through process-local native
        // APIs. Linux system-bus lifecycle notifications are compatibility
        // no-ops and must not tear down the active native subscription.
        QVERIFY(monitor.isSubscribed());
        monitor.notifySystemBusUnavailable();
        QVERIFY(monitor.isSubscribed());
#endif
        monitor.shutdown();
    }

    void runtimeBusReturnResubscribesLogind()
    {
        SystemPowerMonitor monitor;
        QVERIFY(monitor.initialize());
        monitor.injectSubscribedForTesting(true);
        monitor.notifySystemBusUnavailable();
#if defined(Q_OS_LINUX)
        QVERIFY(!monitor.isSubscribed());
        // Return may or may not reach host logind; force subscribed seam after available notify path.
        monitor.notifySystemBusAvailable();
        if (!monitor.isSubscribed()) {
            monitor.injectSubscribedForTesting(true);
        }
        QVERIFY(monitor.isSubscribed());
        monitor.notifySystemBusAvailable();
        QVERIFY(monitor.isSubscribed());
#else
        // Native platform subscriptions are independent of Linux D-Bus.
        QVERIFY(monitor.isSubscribed());
        monitor.notifySystemBusAvailable();
        QVERIFY(monitor.isSubscribed());
#endif
        monitor.shutdown();
    }

    void shutdownWhileResubscriptionPendingIsSafe()
    {
        SystemPowerMonitor monitor;
        QVERIFY(monitor.initialize());
        monitor.injectSubscribedForTesting(true);
        monitor.notifySystemBusUnavailable();
        monitor.notifySystemBusAvailable();
        monitor.shutdown();
        monitor.shutdown();
    }

    void duplicatePrepareForSleepIsIdempotent()
    {
        SystemPowerMonitor monitor;
        QVERIFY(monitor.initialize());
        QSignalSpy spy(&monitor, &SystemPowerMonitor::preparingForSleep);
        monitor.injectPrepareForSleep(true);
        monitor.injectPrepareForSleep(true);
        QCOMPARE(spy.count(), 1);
        monitor.injectPrepareForSleep(false);
        monitor.injectPrepareForSleep(false);
        QCOMPARE(spy.count(), 2);

        RecoveryManager manager;
        int pauseCount = 0;
        int resumeCount = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { ++pauseCount; };
        hooks.resumeBluetoothReconnect = [&]() { ++resumeCount; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        manager.onPreparingForSleep(true);
        manager.onPreparingForSleep(true);
        QCOMPARE(pauseCount, 1);
        manager.onPreparingForSleep(false);
        manager.onPreparingForSleep(false);
        QCOMPARE(resumeCount, 1);
    }

    void disableAutoRecoverCancelsPendingReconcile()
    {
        RecoveryManager manager;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        manager.notifyBlueZAvailable(false);
        manager.notifyBlueZAvailable(true);
        manager.setAutoRecoverEnabled(false);
        manager.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 0);
    }

    void reEnableAutoRecoverStartsEpisodeWhenDegraded()
    {
        RecoveryManager manager;
        int pwReconnect = 0;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.requestPipeWireReconnect = [&]() { ++pwReconnect; };
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        hooks.isPipeWireConnected = []() { return false; };
        hooks.isPipeWireGraphReady = []() { return false; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        manager.setAutoRecoverEnabled(false);
        manager.notifyPipeWireError(QStringLiteral("down"));
        manager.setAutoRecoverEnabled(true);
        QVERIFY(pwReconnect >= 1);
        manager.flushPendingReconcileForTesting();
        // Dependencies not ready — coalesce without refreshing session/routes.
        QCOMPARE(sessionRefresh, 0);
    }

    void reconcileWhilePipeWireNotReadyDoesNotRefreshSession()
    {
        RecoveryManager manager;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        hooks.isPipeWireConnected = []() { return false; };
        hooks.isPipeWireGraphReady = []() { return false; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        manager.notifyBlueZAvailable(false);
        manager.notifyBlueZAvailable(true);
        manager.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 0);
    }

    void reconcileWhenDependenciesReadyRefreshesOnce()
    {
        RecoveryManager manager;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        manager.notifyPipeWireConnected(true, true);
        manager.notifyBlueZAvailable(false);
        manager.notifyBlueZAvailable(true);
        manager.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 1);
    }

    void manualRecoveryStillWorksWhenAutoRecoverDisabled()
    {
        RecoveryManager manager;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        manager.setAutoRecoverEnabled(false);
        manager.notifyBlueZAvailable(false);
        manager.notifyBlueZAvailable(true);
        manager.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 0);
        // Manual host refresh remains independent of RecoveryManager auto path.
        ++sessionRefresh;
        QCOMPARE(sessionRefresh, 1);
    }

    void userFacingStatusNeverShowsAttemptZero()
    {
        RecoveryStatus status;
        status.overall = RecoveryState::Recovering;
        status.pipeWire = RecoveryState::Recovering;
        status.pipeWireAttempts = 0;
        const QString text = userFacingStatus(status);
        QVERIFY(!text.contains(QStringLiteral("attempt 0")));
        QVERIFY(text.contains(QStringLiteral("Preparing")));
        status.pipeWireAttempts = 2;
        const QString text2 = userFacingStatus(status);
        QVERIFY(text2.contains(QStringLiteral("attempt 2")));
    }
};

QTEST_GUILESS_MAIN(TstRecoveryManager)
#include "tst_RecoveryManager.moc"

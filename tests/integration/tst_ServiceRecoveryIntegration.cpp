#include <auralis/recovery/RecoveryManager.h>

#include <QtTest>

using auralis::recovery::RecoveryManager;
using auralis::recovery::RecoveryState;

class TstServiceRecoveryIntegration : public QObject {
    Q_OBJECT

private slots:
    void blueZThenPipeWireBounceCoalescesToHealthy()
    {
        RecoveryManager manager;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = []() {};
        hooks.resumeBluetoothReconnect = []() {};
        hooks.requestBlueZRefresh = []() {};
        hooks.requestPipeWireReconnect = []() {};
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());

        manager.notifyBlueZAvailable(false);
        manager.notifyPipeWireError(QStringLiteral("daemon gone"));
        manager.notifySystemBusConnected(false);
        manager.notifySystemBusConnected(true);
        manager.notifyBlueZAvailable(true);
        manager.notifyPipeWireConnected(true, true);
        manager.flushPendingReconcileForTesting();

        QVERIFY(manager.status().overall == RecoveryState::Healthy);
        QVERIFY(sessionRefresh >= 1);
    }

    void autoRecoverOffBlocksSessionReconcile()
    {
        RecoveryManager manager;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = []() {};
        hooks.resumeBluetoothReconnect = []() {};
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        manager.setHooks(std::move(hooks));
        manager.setAutoRecoverEnabled(false);
        QVERIFY(manager.initialize());
        manager.notifyPipeWireError(QStringLiteral("x"));
        manager.notifyPipeWireConnected(true, true);
        manager.flushPendingReconcileForTesting();
        QCOMPARE(sessionRefresh, 0);
    }

    void suspendStressCycles()
    {
        RecoveryManager manager;
        int pause = 0;
        int resume = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { ++pause; };
        hooks.resumeBluetoothReconnect = [&]() { ++resume; };
        hooks.refreshActiveSession = []() {};
        hooks.requestBlueZRefresh = []() {};
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        for (int i = 0; i < 50; ++i) {
            manager.onPreparingForSleep(true);
            manager.onPreparingForSleep(false);
            manager.flushPendingReconcileForTesting();
        }
        QCOMPARE(pause, 50);
        QCOMPARE(resume, 50);
        QVERIFY(!manager.suspended());
    }

    void blueZChurnStress()
    {
        RecoveryManager manager;
        int sessionRefresh = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = []() {};
        hooks.resumeBluetoothReconnect = []() {};
        hooks.refreshActiveSession = [&]() { ++sessionRefresh; };
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        hooks.isSystemBusConnected = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        for (int i = 0; i < 100; ++i) {
            manager.notifyBlueZAvailable(false);
            manager.notifyBlueZAvailable(true);
        }
        manager.flushPendingReconcileForTesting();
        QVERIFY(sessionRefresh >= 1);
        QVERIFY(manager.status().overall == RecoveryState::Healthy
                || manager.status().overall == RecoveryState::Reconciling
                || manager.status().overall == RecoveryState::Recovering);
    }

    void shutdownDuringRecoveryIsSafe()
    {
        RecoveryManager manager;
        RecoveryManager::HostHooks hooks;
        hooks.refreshActiveSession = []() { QFAIL("should not reconcile after shutdown"); };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        manager.notifyPipeWireError(QStringLiteral("x"));
        manager.shutdown();
        manager.flushPendingReconcileForTesting();
        manager.notifyPipeWireConnected(true, true);
    }
};

QTEST_GUILESS_MAIN(TstServiceRecoveryIntegration)
#include "tst_ServiceRecoveryIntegration.moc"

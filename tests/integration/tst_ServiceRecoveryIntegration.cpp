#include <auralis/recovery/RecoveryManager.h>

#include <QSignalSpy>
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
        int pwReconnect = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = []() {};
        hooks.resumeBluetoothReconnect = []() {};
        hooks.requestBlueZRefresh = []() {};
        hooks.requestPipeWireReconnect = [&]() { ++pwReconnect; };
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

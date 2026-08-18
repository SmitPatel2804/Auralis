#include <auralis/audio/PipeWireManager.h>
#include <auralis/core/ServiceStatus.h>

#include <QtTest>

using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireManager;
using auralis::core::ServiceStatus;

class TstPipeWireManager : public QObject {
    Q_OBJECT

private slots:
    void lifecycleWithoutRequiringLiveServer()
    {
        PipeWireManager manager;
        QVERIFY(manager.status() == ServiceStatus::Uninitialized);
        QVERIFY(manager.connectionState() == PipeWireConnectionState::Stopped);
        const bool started = manager.initialize();
        if (started) {
            QVERIFY(manager.status() != ServiceStatus::Uninitialized);
        } else {
            QVERIFY(manager.status() == ServiceStatus::Error);
            QVERIFY(manager.connectionState() == PipeWireConnectionState::Error);
            QVERIFY(!manager.lastError().isEmpty());
        }
        manager.shutdown();
        QVERIFY(manager.status() == ServiceStatus::Uninitialized);
        QVERIFY(manager.connectionState() == PipeWireConnectionState::Stopped);
        manager.shutdown();
    }

    void repeatedStartStopIsSafe()
    {
        PipeWireManager manager;
        manager.initialize();
        manager.shutdown();
        manager.initialize();
        manager.shutdown();
        QCOMPARE(manager.endpointCount(), 0);
        QVERIFY(manager.status() == ServiceStatus::Uninitialized);
        QVERIFY(manager.connectionState() == PipeWireConnectionState::Stopped);
    }
};

QTEST_GUILESS_MAIN(TstPipeWireManager)
#include "tst_PipeWireManager.moc"

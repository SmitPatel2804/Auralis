#include <auralis/audio/PipeWireManager.h>
#include <auralis/core/ServiceStatus.h>

#include <QtTest>

class TstPipeWireManager : public QObject {
    Q_OBJECT

private slots:
    void lifecycle()
    {
        auralis::audio::PipeWireManager manager;
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
        QVERIFY(manager.initialize());
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Ready);
        manager.shutdown();
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
    }
};

QTEST_GUILESS_MAIN(TstPipeWireManager)
#include "tst_PipeWireManager.moc"

#include <auralis/core/ServiceStatus.h>
#include <auralis/session/SessionManager.h>

#include <QtTest>

class TstSessionManager : public QObject {
    Q_OBJECT

private slots:
    void lifecycle()
    {
        auralis::session::SessionManager manager;
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
        QVERIFY(manager.initialize());
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Ready);
        manager.shutdown();
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
    }
};

QTEST_GUILESS_MAIN(TstSessionManager)
#include "tst_SessionManager.moc"

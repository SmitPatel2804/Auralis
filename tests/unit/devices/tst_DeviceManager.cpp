#include <auralis/core/ServiceStatus.h>
#include <auralis/devices/DeviceManager.h>

#include <QtTest>

class TstDeviceManager : public QObject {
    Q_OBJECT

private slots:
    void lifecycle()
    {
        auralis::devices::DeviceManager manager;
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
        QVERIFY(manager.initialize());
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Ready);
        manager.shutdown();
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
    }
};

QTEST_GUILESS_MAIN(TstDeviceManager)
#include "tst_DeviceManager.moc"

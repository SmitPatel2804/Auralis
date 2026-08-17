#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/core/ServiceStatus.h>

#include <QtTest>

class TstBluetoothManager : public QObject {
    Q_OBJECT

private slots:
    void lifecycle()
    {
        auralis::bluetooth::BluetoothManager manager;
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
        QVERIFY(manager.initialize());
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Ready);
        manager.shutdown();
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
        manager.shutdown();
        QVERIFY(manager.status() == auralis::core::ServiceStatus::Uninitialized);
    }
};

QTEST_GUILESS_MAIN(TstBluetoothManager)
#include "tst_BluetoothManager.moc"

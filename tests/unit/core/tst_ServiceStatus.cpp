#include <auralis/core/ServiceStatus.h>

#include <QtTest>

class TstServiceStatus : public QObject {
    Q_OBJECT

private slots:
    void convertsKnownValues()
    {
        using auralis::core::ServiceStatus;
        using auralis::core::toString;

        QCOMPARE(toString(ServiceStatus::Uninitialized), QStringLiteral("Uninitialized"));
        QCOMPARE(toString(ServiceStatus::Initializing), QStringLiteral("Initializing"));
        QCOMPARE(toString(ServiceStatus::Ready), QStringLiteral("Ready"));
        QCOMPARE(toString(ServiceStatus::Error), QStringLiteral("Error"));
    }
};

QTEST_GUILESS_MAIN(TstServiceStatus)
#include "tst_ServiceStatus.moc"

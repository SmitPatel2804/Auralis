#include <auralis/bluetooth/BluetoothDbusError.h>
#include <auralis/bluetooth/BluetoothError.h>
#include <auralis/bluetooth/BlueZConstants.h>

#include <QtTest>

using auralis::bluetooth::BluetoothError;
using auralis::bluetooth::mapDbusError;

class TstBluetoothDbusError : public QObject {
    Q_OBJECT

private slots:
    void mapsAuthenticationFailed()
    {
        const auto mapped = mapDbusError(
            auralis::bluetooth::bluez::kErrorAuthenticationFailed.toString(),
            QStringLiteral("Authentication failed"));
        QVERIFY(mapped.category == BluetoothError::AuthenticationFailed);
        QCOMPARE(
            mapped.dbusErrorName,
            auralis::bluetooth::bluez::kErrorAuthenticationFailed.toString());
    }

    void mapsInProgress()
    {
        const auto mapped = mapDbusError(auralis::bluetooth::bluez::kErrorInProgress.toString(), {});
        QVERIFY(mapped.category == BluetoothError::InProgress);
        QVERIFY(mapped.retryable);
    }

    void mapsAlreadyConnected()
    {
        const auto mapped = mapDbusError(auralis::bluetooth::bluez::kErrorAlreadyConnected.toString(), {});
        QVERIFY(mapped.category == BluetoothError::AlreadyConnected);
    }

    void mapsUnknownToDbusCallFailed()
    {
        const auto mapped = mapDbusError(QStringLiteral("org.example.Error.Custom"), QStringLiteral("boom"));
        QVERIFY(mapped.category == BluetoothError::DbusCallFailed);
        QCOMPARE(mapped.message, QStringLiteral("boom"));
    }
};

QTEST_GUILESS_MAIN(TstBluetoothDbusError)
#include "tst_BluetoothDbusError.moc"

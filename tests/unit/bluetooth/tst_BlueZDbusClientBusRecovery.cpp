#include <auralis/bluetooth/BlueZDbusClient.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::BlueZDbusClient;

class TstBlueZDbusClientBusRecovery : public QObject {
    Q_OBJECT

private slots:
    void injectDisconnectReconnectIsIdempotent()
    {
        BlueZDbusClient client;
        QVERIFY(client.initialize());

        QSignalSpy busSpy(&client, &BlueZDbusClient::systemBusStateChanged);
        const bool initiallyConnected = client.isSystemBusConnected();
        if (!initiallyConnected) {
            // Environment without system bus — still validate inject seam.
            client.injectSystemBusConnectedForTesting(true);
            QVERIFY(client.isSystemBusConnected());
            client.injectSystemBusConnectedForTesting(true);
            QCOMPARE(busSpy.count(), 1);
            client.injectSystemBusConnectedForTesting(false);
            QVERIFY(!client.isSystemBusConnected());
            client.injectSystemBusConnectedForTesting(false);
            QCOMPARE(busSpy.count(), 2);
            client.shutdown();
            return;
        }

        const int genBefore = client.busAttachGenerationForTesting();
        client.injectSystemBusConnectedForTesting(false);
        QVERIFY(!client.isSystemBusConnected());
        QVERIFY(!client.isBlueZAvailable());
        client.injectSystemBusConnectedForTesting(false);
        QCOMPARE(busSpy.count(), 1);

        client.injectSystemBusConnectedForTesting(true);
        QVERIFY(client.isSystemBusConnected());
        QVERIFY(client.busAttachGenerationForTesting() >= genBefore);
        client.injectSystemBusConnectedForTesting(true);
        QCOMPARE(busSpy.count(), 2);
        client.shutdown();
    }

    void shutdownWhileDisconnectedIsSafe()
    {
        BlueZDbusClient client;
        QVERIFY(client.initialize());
        client.injectSystemBusConnectedForTesting(false);
        client.shutdown();
        client.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstBlueZDbusClientBusRecovery)
#include "tst_BlueZDbusClientBusRecovery.moc"

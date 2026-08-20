#include <auralis/bluetooth/BlueZDbusClient.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::bluetooth::BlueZDbusClient;

class TstBlueZDbusClientBusRecovery : public QObject {
    Q_OBJECT

private slots:
    void busHealthMonitoringContinuesWhileHealthy()
    {
        BlueZDbusClient client;
        client.setSystemBusConnectedOverrideForTesting(true);
        QVERIFY(client.initialize());
        QVERIFY(client.isSystemBusConnected());
        QVERIFY(client.busHealthTimerActiveForTesting());
        client.shutdown();
    }

    void runtimeBusDisconnectIsDetectedViaProbe()
    {
        BlueZDbusClient client;
        client.setSystemBusConnectedOverrideForTesting(true);
        QVERIFY(client.initialize());
        QVERIFY(client.isSystemBusConnected());
        QVERIFY(client.busHealthTimerActiveForTesting());

        QSignalSpy busSpy(&client, &BlueZDbusClient::systemBusStateChanged);
        client.setSystemBusConnectedOverrideForTesting(false);
        client.pollSystemBusHealthForTesting();
        QVERIFY(!client.isSystemBusConnected());
        QCOMPARE(busSpy.count(), 1);

        client.pollSystemBusHealthForTesting();
        QCOMPARE(busSpy.count(), 1);
        client.shutdown();
    }

    void runtimeBusReconnectRebuildsGeneration()
    {
        BlueZDbusClient client;
        client.setSystemBusConnectedOverrideForTesting(true);
        QVERIFY(client.initialize());
        const int genHealthy = client.busAttachGenerationForTesting();

        client.setSystemBusConnectedOverrideForTesting(false);
        client.pollSystemBusHealthForTesting();
        QVERIFY(!client.isSystemBusConnected());

        client.setSystemBusConnectedOverrideForTesting(true);
        client.pollSystemBusHealthForTesting();
        QVERIFY(client.isSystemBusConnected());
        QVERIFY(client.busAttachGenerationForTesting() > genHealthy);
        QVERIFY(client.busHealthTimerActiveForTesting());
        client.shutdown();
    }

    void injectDisconnectReconnectIsIdempotent()
    {
        BlueZDbusClient client;
        QVERIFY(client.initialize());

        QSignalSpy busSpy(&client, &BlueZDbusClient::systemBusStateChanged);
        client.injectSystemBusConnectedForTesting(true);
        const int afterUp = busSpy.count();
        client.injectSystemBusConnectedForTesting(true);
        QCOMPARE(busSpy.count(), afterUp);

        client.injectSystemBusConnectedForTesting(false);
        QVERIFY(!client.isSystemBusConnected());
        const int afterDown = busSpy.count();
        client.injectSystemBusConnectedForTesting(false);
        QCOMPARE(busSpy.count(), afterDown);
        client.shutdown();
    }

    void oldGenerationSnapshotIsIgnored()
    {
        BlueZDbusClient client;
        client.setSystemBusConnectedOverrideForTesting(true);
        QVERIFY(client.initialize());
        client.setBlueZAvailableForTesting(true);

        QSignalSpy snapSpy(&client, &BlueZDbusClient::snapshotReceived);
        client.requestSnapshot();
        QVERIFY(client.snapshotInFlightForTesting());
        const quint64 staleGen = static_cast<quint64>(client.busAttachGenerationForTesting());

        client.setSystemBusConnectedOverrideForTesting(false);
        client.pollSystemBusHealthForTesting();
        client.setSystemBusConnectedOverrideForTesting(true);
        client.pollSystemBusHealthForTesting();
        client.setBlueZAvailableForTesting(true);

        QVariantMap objects;
        objects.insert(QStringLiteral("/org/bluez"), QVariantMap{});
        client.injectSnapshotFinishedForTesting(staleGen, objects);
        QCOMPARE(snapSpy.count(), 0);
        client.shutdown();
    }

    void snapshotAfterShutdownIsIgnored()
    {
        BlueZDbusClient client;
        client.setSystemBusConnectedOverrideForTesting(true);
        QVERIFY(client.initialize());
        client.setBlueZAvailableForTesting(true);
        client.requestSnapshot();
        const quint64 gen = static_cast<quint64>(client.busAttachGenerationForTesting());
        QSignalSpy snapSpy(&client, &BlueZDbusClient::snapshotReceived);
        client.shutdown();
        QVariantMap objects;
        objects.insert(QStringLiteral("/x"), QVariantMap{});
        client.injectSnapshotFinishedForTesting(gen, objects);
        QCOMPARE(snapSpy.count(), 0);
    }

    void snapshotRequestCoalescesWhileInFlight()
    {
        BlueZDbusClient client;
        client.setSystemBusConnectedOverrideForTesting(true);
        QVERIFY(client.initialize());
        client.setBlueZAvailableForTesting(true);

        QSignalSpy snapSpy(&client, &BlueZDbusClient::snapshotReceived);
        client.requestSnapshot();
        QVERIFY(client.snapshotInFlightForTesting());
        client.requestSnapshot();
        client.requestSnapshot();
        QVERIFY(client.snapshotInFlightForTesting());

        const quint64 gen = static_cast<quint64>(client.busAttachGenerationForTesting());
        QVariantMap objects;
        objects.insert(QStringLiteral("/a"), QVariantMap{});
        client.injectSnapshotFinishedForTesting(gen, objects);
        QCOMPARE(snapSpy.count(), 1);
        // Pending refresh re-issues one more in-flight call.
        QVERIFY(client.snapshotInFlightForTesting());
        client.injectSnapshotFinishedForTesting(gen, objects);
        QCOMPARE(snapSpy.count(), 2);
        QVERIFY(!client.snapshotInFlightForTesting());
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

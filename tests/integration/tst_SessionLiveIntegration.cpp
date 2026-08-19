#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/session/SessionManager.h>

#include <QTemporaryDir>
#include <QtTest>

using auralis::bluetooth::BluetoothManager;
using auralis::audio::PipeWireManager;
using auralis::session::SessionManager;

class TstSessionLiveIntegration : public QObject {
    Q_OBJECT

private slots:
    void sessionManagerInitializesWithLiveServices()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_SESSION_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_SESSION_INTEGRATION=1 to run live session tests");
        }

        BluetoothManager bluetooth;
        PipeWireManager pipeWire(bluetooth.deviceRegistry());
        QTemporaryDir tempDir;
        SessionManager sessions(&bluetooth, &pipeWire, tempDir.filePath(QStringLiteral("sessions.json")));

        QVERIFY(bluetooth.initialize());
        QVERIFY(pipeWire.initialize());
        QVERIFY(sessions.initialize());
        QCOMPARE(sessions.sessionCount(), 0);

        sessions.shutdown();
        pipeWire.shutdown();
        bluetooth.shutdown();
    }

    void twoDeviceLiveSessionIfConfigured()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_SESSION_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_SESSION_INTEGRATION=1 to run live session tests");
        }

        const QString raw = QString::fromLocal8Bit(qgetenv("AURALIS_EXPECT_DEVICE_ADDRESSES")).trimmed();
        const QStringList addresses = raw.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        if (addresses.size() < 2) {
            QSKIP("Set AURALIS_EXPECT_DEVICE_ADDRESSES=\"AA:BB:CC:DD:EE:FF;11:22:33:44:55:66\" "
                  "for two-device live session coverage");
        }

        BluetoothManager bluetooth;
        PipeWireManager pipeWire(bluetooth.deviceRegistry());
        QTemporaryDir tempDir;
        SessionManager sessions(&bluetooth, &pipeWire, tempDir.filePath(QStringLiteral("sessions.json")));
        QVERIFY(bluetooth.initialize());
        QVERIFY(pipeWire.initialize());
        QVERIFY(sessions.initialize());

        const QString id = sessions.createSession(QStringLiteral("Live Pair"));
        QVERIFY(sessions.addDevice(id, addresses.at(0).trimmed(), QStringLiteral("Left"))
                == auralis::session::SessionCommandResult::Accepted);
        QVERIFY(sessions.addDevice(id, addresses.at(1).trimmed(), QStringLiteral("Right"))
                == auralis::session::SessionCommandResult::Accepted);

        sessions.shutdown();
        pipeWire.shutdown();
        bluetooth.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstSessionLiveIntegration)
#include "tst_SessionLiveIntegration.moc"

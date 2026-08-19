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
};

QTEST_GUILESS_MAIN(TstSessionLiveIntegration)
#include "tst_SessionLiveIntegration.moc"

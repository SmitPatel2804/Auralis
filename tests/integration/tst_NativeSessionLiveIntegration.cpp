#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/NativeAudioManager.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/NativeBluetoothManager.h>
#include <auralis/session/SessionManager.h>

#include <QTemporaryDir>
#include <QtTest>

class TstNativeSessionLiveIntegration final : public QObject {
    Q_OBJECT

private slots:
    void mutedSessionLifecycleOnConnectedBluetoothDevice()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_NATIVE_SESSION_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_NATIVE_SESSION_INTEGRATION=1 to exercise a real native session");
        }

        const QString expectedAddress =
            QString::fromLocal8Bit(qgetenv("AURALIS_EXPECT_DEVICE_ADDRESS")).trimmed().toUpper();
        QVERIFY2(!expectedAddress.isEmpty(), "AURALIS_EXPECT_DEVICE_ADDRESS is required");

        auralis::bluetooth::NativeBluetoothManager bluetooth;
        QVERIFY(bluetooth.initialize());
        bluetooth.startScan();
        QTRY_VERIFY_WITH_TIMEOUT(bluetooth.scanning(), 3000);

        const auto expectedDeviceId = [&bluetooth, &expectedAddress]() {
            for (const auto& device : bluetooth.deviceRegistry()->devices()) {
                if (device.address.trimmed().toUpper() == expectedAddress) {
                    return device.objectPath;
                }
            }
            return QString();
        };
        QTRY_VERIFY_WITH_TIMEOUT(!expectedDeviceId().isEmpty(), 12000);
        bluetooth.stopScan();
        const auto* device = bluetooth.deviceRegistry()->findByObjectPath(expectedDeviceId());
        QVERIFY(device != nullptr);
        QVERIFY(device->paired);
        QVERIFY(device->connected);

        auralis::audio::NativeAudioManager audio(bluetooth.deviceRegistry());
        QVERIFY(audio.initialize());
        QTRY_VERIFY_WITH_TIMEOUT(audio.mappedBluetoothCount() >= 1, 3000);
        QVERIFY(!audio.audioRouter()->sourceList().isEmpty());

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        auralis::session::SessionManager sessions(
            &bluetooth,
            &audio,
            tempDir.filePath(QStringLiteral("native-sessions.json")));
        QVERIFY(sessions.initialize());

        using Result = auralis::session::SessionCommandResult;
        using State = auralis::session::SessionState;

        const QString sessionId = sessions.createSession(QStringLiteral("Smokin Buds live validation"));
        QVERIFY(!sessionId.isEmpty());
        QVERIFY(sessions.renameSession(sessionId, QStringLiteral("Smokin Buds muted route")) == Result::Accepted);
        QVERIFY(sessions.addDevice(sessionId, expectedAddress) == Result::Accepted);
        QVERIFY(sessions.setSource(sessionId, audio.audioRouter()->sourceList().constFirst().id) == Result::Accepted);
        QVERIFY(sessions.setSessionMuted(sessionId, true) == Result::Accepted);
        QVERIFY(sessions.setGroupVolume(sessionId, 0.37) == Result::Accepted);
        QVERIFY(sessions.setDeviceVolume(sessionId, expectedAddress, 0.42) == Result::Accepted);

        QVERIFY(sessions.activateSession(sessionId) == Result::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(
            sessions.sessionById(sessionId).has_value()
                && sessions.sessionById(sessionId)->state == State::Active,
            5000);
        QCOMPARE(audio.audioRouter()->ownedLinkCount(), 1);
        QVERIFY(sessions.sessionById(sessionId)->muted);
        QCOMPARE(sessions.sessionById(sessionId)->groupVolume, 0.37);

        QVERIFY(sessions.deactivateSession(sessionId) == Result::Accepted);
        QTRY_VERIFY_WITH_TIMEOUT(
            sessions.sessionById(sessionId).has_value()
                && sessions.sessionById(sessionId)->state == State::Idle,
            3000);
        QCOMPARE(audio.audioRouter()->ownedLinkCount(), 0);

        const QString duplicateId = sessions.duplicateSession(sessionId);
        QVERIFY(!duplicateId.isEmpty());
        QVERIFY(sessions.deleteSession(duplicateId) == Result::Accepted);
        QVERIFY(sessions.deleteSession(sessionId) == Result::Accepted);
        QCOMPARE(sessions.sessionCount(), 0);

        sessions.shutdown();
        audio.shutdown();
        bluetooth.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstNativeSessionLiveIntegration)
#include "tst_NativeSessionLiveIntegration.moc"

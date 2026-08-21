#include "WindowsAudioSessions.h"
#include "WindowsProcessLoopbackCapture.h"

#include <QAudioFormat>
#include <QMutex>
#include <QProcess>
#include <QSet>
#include <QTest>

#include <atomic>
#include <algorithm>

class TstWindowsProcessLoopback final : public QObject {
    Q_OBJECT

private slots:
    void sessionsAreUniqueApplications()
    {
        QString error;
        const auto sessions = auralis::audio::enumerateWindowsAudioSessions(&error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QSet<quint32> processIds;
        for (const auto& session : sessions) {
            QVERIFY(session.processId != 0);
            QVERIFY(!session.applicationName.trimmed().isEmpty());
            QVERIFY2(!processIds.contains(session.processId), "Duplicate application PID exposed in Sessions");
            processIds.insert(session.processId);
        }
    }

    void activeApplicationProducesLoopbackPackets()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_PROCESS_LOOPBACK_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_PROCESS_LOOPBACK_INTEGRATION=1 while an application is playing audio");
        }

        QProcess fixture;
        fixture.start(QString::fromUtf8(AURALIS_AUDIO_FIXTURE_PATH));
        QVERIFY2(fixture.waitForStarted(3000), qPrintable(fixture.errorString()));
        auralis::audio::WindowsAudioSession active;
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            QString enumerationError;
            const auto sessions = auralis::audio::enumerateWindowsAudioSessions(&enumerationError);
            if (!enumerationError.isEmpty()) return false;
            const auto match = std::find_if(sessions.cbegin(), sessions.cend(), [&fixture](const auto& session) {
                return session.active && session.processId == static_cast<quint32>(fixture.processId());
            });
            if (match == sessions.cend()) return false;
            active = *match;
            return true;
        })(), 4000);

        QAudioFormat format;
        format.setSampleRate(48000);
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Float);
        std::atomic<quint64> capturedBytes = 0;
        QMutex errorMutex;
        QString asynchronousError;
        auralis::audio::WindowsProcessLoopbackCapture capture;
        QString startError;
        QVERIFY2(
            capture.start(
                active.processId,
                format,
                [&capturedBytes](QByteArray pcm) {
                    capturedBytes.fetch_add(static_cast<quint64>(pcm.size()), std::memory_order_relaxed);
                },
                [&errorMutex, &asynchronousError](QString error) {
                    QMutexLocker locker(&errorMutex);
                    asynchronousError = std::move(error);
                },
                &startError),
            qPrintable(startError));
        QTRY_VERIFY_WITH_TIMEOUT(capturedBytes.load(std::memory_order_relaxed) > 0, 5000);
        capture.stop();
        QVERIFY(!capture.running());
        fixture.terminate();
        if (!fixture.waitForFinished(1000)) {
            fixture.kill();
            QVERIFY(fixture.waitForFinished(2000));
        }
        QMutexLocker locker(&errorMutex);
        QVERIFY2(asynchronousError.isEmpty(), qPrintable(asynchronousError));
    }
};

QTEST_GUILESS_MAIN(TstWindowsProcessLoopback)
#include "tst_WindowsProcessLoopback.moc"

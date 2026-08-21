#include "WindowsEndpointLoopbackCapture.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QMutex>
#include <QProcess>
#include <QTest>

#include <atomic>

class TstWindowsEndpointLoopback final : public QObject {
    Q_OBJECT

private slots:
    void rejectsInvalidEndpoint()
    {
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Float);
        auralis::audio::WindowsEndpointLoopbackCapture capture;
        QString error;
        QVERIFY(!capture.start({}, format, {}, {}, &error));
        QVERIFY(!error.isEmpty());
    }

    void capturesDefaultRenderEndpoint()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_ENDPOINT_LOOPBACK_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_ENDPOINT_LOOPBACK_INTEGRATION=1 to open the default Windows render endpoint");
        }

        const QAudioDevice output = QMediaDevices::defaultAudioOutput();
        QVERIFY2(!output.isNull(), "Windows has no default render endpoint");
        QProcess fixture;
        fixture.start(QString::fromUtf8(AURALIS_AUDIO_FIXTURE_PATH));
        QVERIFY2(fixture.waitForStarted(3000), qPrintable(fixture.errorString()));

        QAudioFormat format;
        format.setSampleRate(48000);
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Float);
        std::atomic<quint64> capturedBytes = 0;
        QMutex errorMutex;
        QString asynchronousError;
        auralis::audio::WindowsEndpointLoopbackCapture capture;
        QString startError;
        QVERIFY2(
            capture.start(
                output.id(),
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

QTEST_GUILESS_MAIN(TstWindowsEndpointLoopback)
#include "tst_WindowsEndpointLoopback.moc"

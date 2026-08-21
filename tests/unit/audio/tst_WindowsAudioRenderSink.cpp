#include "WindowsAudioRenderSink.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QtTest>

class TstWindowsAudioRenderSink final : public QObject {
    Q_OBJECT

private slots:
    void opensAndWritesEveryActiveRenderEndpoint()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_WINDOWS_RENDER_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_WINDOWS_RENDER_INTEGRATION=1 to open every active Windows render endpoint");
        }

        const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
        QVERIFY2(!outputs.isEmpty(), "Windows has no active render endpoints");
        for (const QAudioDevice& output : outputs) {
            QAudioFormat format = output.preferredFormat();
            QVERIFY2(format.isValid(), qPrintable(QStringLiteral("Invalid preferred format for %1").arg(output.description())));

            auralis::audio::WindowsAudioRenderSink sink;
            QString error;
            QVERIFY2(
                sink.start(output.id(), output.description(), format, {}, &error),
                qPrintable(QStringLiteral("%1: %2").arg(output.description(), error)));
            sink.setVolume(0.0);
            QVERIFY(sink.running());
            QVERIFY(sink.bufferSize() > 0);

            const QByteArray silence(format.bytesForDuration(10000), '\0');
            QVERIFY(!silence.isEmpty());
            qint64 written = 0;
            for (int packet = 0; packet < 20; ++packet) {
                written += sink.write(silence);
                QTest::qWait(10);
            }
            QVERIFY2(written > 0, qPrintable(QStringLiteral("No audio accepted by %1").arg(output.description())));
            QVERIFY(sink.running());
            qInfo() << "validated Windows output" << output.description()
                    << "queueMs=" << format.durationForBytes(sink.bufferSize()) / 1000.0
                    << "droppedBytes=" << sink.droppedBytes();
            sink.stop();
        }
    }
};

QTEST_GUILESS_MAIN(TstWindowsAudioRenderSink)
#include "tst_WindowsAudioRenderSink.moc"

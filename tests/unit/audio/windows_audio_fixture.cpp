#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QCoreApplication>
#include <QIODevice>
#include <QMediaDevices>
#include <QTimer>

#include <algorithm>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QAudioDevice output = QMediaDevices::defaultAudioOutput();
    if (output.isNull()) return 2;
    QAudioFormat format = output.preferredFormat();
    if (!format.isValid()) return 3;

    QAudioSink sink(output, format);
    sink.setVolume(0.0);
    QIODevice* stream = sink.start();
    if (stream == nullptr) return 4;
    const qsizetype frameBytes = std::max(1, format.bytesPerFrame());
    const qsizetype blockBytes = std::max<qsizetype>(frameBytes, format.bytesForDuration(20000));
    const QByteArray silence(blockBytes / frameBytes * frameBytes, '\0');
    QTimer writer;
    QObject::connect(&writer, &QTimer::timeout, &app, [stream, silence] { stream->write(silence); });
    writer.start(10);
    QTimer::singleShot(8000, &app, &QCoreApplication::quit);
    return app.exec();
}

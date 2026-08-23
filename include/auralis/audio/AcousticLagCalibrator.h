#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>
#include <memory>

namespace auralis::audio {

class AudioEndpointRegistry;
class AudioRouter;
class IPipeWireLinkBackend;
class PipeWireObjectStore;
struct PipeWireLagCapture;

/// Timeshares each session destination for up to 10 seconds, correlates the
/// laptop microphone against Auralis Virtual Output, then pads the faster path.
class AcousticLagCalibrator final : public QObject {
    Q_OBJECT

public:
    AcousticLagCalibrator(
        AudioRouter* router,
        PipeWireObjectStore* store,
        AudioEndpointRegistry* endpoints,
        IPipeWireLinkBackend* backend,
        QObject* parent = nullptr);
    ~AcousticLagCalibrator() override;

    QString statusText() const;
    bool running() const noexcept { return running_; }

    void maybeStart(const QString& sessionId, const QStringList& endpointIds);
    void stop();

signals:
    void statusChanged();
    void finished();

private:
    void onSampleTimer();
    bool startCapture();
    void stopCapture();
    void applySolo(int index);
    void restoreMute();
    QVector<float> snapshot(bool reference) const;

    AudioRouter* router_ = nullptr;
    PipeWireObjectStore* store_ = nullptr;
    AudioEndpointRegistry* endpoints_ = nullptr;
    IPipeWireLinkBackend* backend_ = nullptr;
    QStringList endpointIds_;
    QString sessionId_;
    QString status_;
    bool running_ = false;
    int phaseIndex_ = 0;
    qint64 phaseStartedMs_ = 0;
    qint64 startedMs_ = 0;
    int sampleRateHz_ = 48000;
    QObject* sampleTimer_ = nullptr;
    QSet<QString> calibratedSessions_;
    QString ignoreSession_;
    std::unique_ptr<PipeWireLagCapture> capture_;
};

} // namespace auralis::audio

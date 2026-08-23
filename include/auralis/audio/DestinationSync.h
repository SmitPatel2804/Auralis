#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include <cstdint>
#include <optional>

namespace auralis::audio {

struct DestinationLatencySample {
    QString endpointId;
    std::optional<qint64> inputLatencyNs;
};

/// Spec 7.2: delay faster destinations toward the slowest observed host latency.
/// Unknown samples are left at 0. Differences below 1 ms are ignored.
QHash<QString, double> destinationDelayMsTowardMax(const QVector<DestinationLatencySample>& samples);

/// Cross-correlation of mic (observed) vs the mix (reference). Positive milliseconds
/// mean the observed path is behind the reference (device + air delay).
std::optional<double> estimateLagMs(
    const QVector<float>& reference,
    const QVector<float>& observed,
    int sampleRateHz,
    double maxLagMs = 250.0);

struct LagObservation {
    QString endpointId;
    double delayMs = 0.0;
    qint64 timestampMs = 0;
};

/// Rolling mean of per-device lag samples. Observations older than 10 seconds
/// are dropped so a finished window is the average of up to 10 s of runtime data.
class RuntimeLagAverager {
public:
    static constexpr qint64 kWindowMs = 10000;

    void add(const QString& endpointId, double delayMs, qint64 nowMs);
    void clear();
    bool hasTwoEndpoints(qint64 nowMs) const;
    QHash<QString, double> averagedDelayMs(qint64 nowMs) const;
    QVector<DestinationLatencySample> averagedLatencySamples(qint64 nowMs) const;

private:
    void prune(qint64 nowMs) const;
    mutable QVector<LagObservation> observations_;
};

} // namespace auralis::audio

#include <auralis/audio/DestinationSync.h>

#include <algorithm>
#include <cmath>

namespace auralis::audio {
namespace {

constexpr double kMinCompensationMs = 1.0;
constexpr double kMaxCompensationMs = 250.0;

} // namespace

QHash<QString, double> destinationDelayMsTowardMax(const QVector<DestinationLatencySample>& samples)
{
    QHash<QString, double> delays;
    QVector<const DestinationLatencySample*> known;
    known.reserve(samples.size());
    for (const DestinationLatencySample& sample : samples) {
        if (sample.endpointId.isEmpty()) {
            continue;
        }
        delays.insert(sample.endpointId, 0.0);
        if (sample.inputLatencyNs.has_value() && *sample.inputLatencyNs >= 0) {
            known.push_back(&sample);
        }
    }
    if (known.size() < 2) {
        return delays;
    }

    qint64 maxNs = 0;
    for (const DestinationLatencySample* sample : known) {
        maxNs = std::max(maxNs, *sample->inputLatencyNs);
    }

    for (const DestinationLatencySample* sample : known) {
        const double delayMs = static_cast<double>(maxNs - *sample->inputLatencyNs) / 1'000'000.0;
        if (!std::isfinite(delayMs) || delayMs < kMinCompensationMs) {
            continue;
        }
        delays.insert(sample->endpointId, std::min(delayMs, kMaxCompensationMs));
    }
    return delays;
}

} // namespace auralis::audio

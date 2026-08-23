#include <auralis/audio/DestinationSync.h>

#include <algorithm>
#include <cmath>

namespace auralis::audio {
namespace {

constexpr double kMinCompensationMs = 1.0;
constexpr double kMaxCompensationMs = 250.0;
constexpr int kCorrelationRateHz = 8000;
constexpr double kMinRms = 0.008;
constexpr double kMinPeak = 0.18;

QVector<float> downsample(const QVector<float>& input, int sampleRateHz, int outRateHz)
{
    if (input.isEmpty() || sampleRateHz <= 0 || outRateHz <= 0) {
        return {};
    }
    if (sampleRateHz <= outRateHz) {
        return input;
    }
    const int factor = sampleRateHz / outRateHz;
    if (factor <= 1) {
        return input;
    }
    QVector<float> output;
    output.reserve(input.size() / factor);
    for (int i = 0; i + factor <= input.size(); i += factor) {
        double sum = 0.0;
        for (int k = 0; k < factor; ++k) {
            sum += static_cast<double>(input.at(i + k));
        }
        output.push_back(static_cast<float>(sum / static_cast<double>(factor)));
    }
    return output;
}

double rms(const QVector<float>& samples)
{
    if (samples.isEmpty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (float sample : samples) {
        const double value = static_cast<double>(sample);
        sum += value * value;
    }
    return std::sqrt(sum / static_cast<double>(samples.size()));
}

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

std::optional<double> estimateLagMs(
    const QVector<float>& reference,
    const QVector<float>& observed,
    int sampleRateHz,
    double maxLagMs)
{
    if (sampleRateHz <= 0 || !std::isfinite(maxLagMs) || maxLagMs <= 0.0) {
        return std::nullopt;
    }
    const QVector<float> ref = downsample(reference, sampleRateHz, kCorrelationRateHz);
    const QVector<float> obs = downsample(observed, sampleRateHz, kCorrelationRateHz);
    const int rate = std::min(sampleRateHz, kCorrelationRateHz);
    if (ref.size() < 64 || obs.size() < 64) {
        return std::nullopt;
    }
    if (rms(ref) < kMinRms || rms(obs) < kMinRms) {
        return std::nullopt;
    }

    const int maxLag = std::max(1, static_cast<int>(std::lround(maxLagMs * static_cast<double>(rate) / 1000.0)));
    const int available = std::min(ref.size(), obs.size());
    const int span = available - maxLag;
    if (span < 32) {
        return std::nullopt;
    }

    double best = -1.0;
    int bestLag = 0;
    for (int lag = 0; lag <= maxLag; ++lag) {
        double dot = 0.0;
        double energyRef = 0.0;
        double energyObs = 0.0;
        for (int i = 0; i < span; ++i) {
            const double a = static_cast<double>(ref.at(i));
            const double b = static_cast<double>(obs.at(i + lag));
            dot += a * b;
            energyRef += a * a;
            energyObs += b * b;
        }
        const double denom = std::sqrt(energyRef * energyObs);
        if (denom < 1.0e-12) {
            continue;
        }
        const double ncc = dot / denom;
        if (ncc > best) {
            best = ncc;
            bestLag = lag;
        }
    }
    if (best < kMinPeak) {
        return std::nullopt;
    }
    return static_cast<double>(bestLag) * 1000.0 / static_cast<double>(rate);
}

void RuntimeLagAverager::add(const QString& endpointId, double delayMs, qint64 nowMs)
{
    if (endpointId.isEmpty() || !std::isfinite(delayMs) || delayMs < 0.0) {
        return;
    }
    observations_.push_back({endpointId, std::clamp(delayMs, 0.0, kMaxCompensationMs), nowMs});
    prune(nowMs);
}

void RuntimeLagAverager::clear()
{
    observations_.clear();
}

void RuntimeLagAverager::prune(qint64 nowMs) const
{
    QVector<LagObservation> kept;
    kept.reserve(observations_.size());
    for (const LagObservation& observation : observations_) {
        if (observation.timestampMs > nowMs) {
            continue;
        }
        if (nowMs - observation.timestampMs <= kWindowMs) {
            kept.push_back(observation);
        }
    }
    observations_ = std::move(kept);
}

bool RuntimeLagAverager::hasTwoEndpoints(qint64 nowMs) const
{
    return averagedDelayMs(nowMs).size() >= 2;
}

QHash<QString, double> RuntimeLagAverager::averagedDelayMs(qint64 nowMs) const
{
    prune(nowMs);
    QHash<QString, double> sums;
    QHash<QString, int> counts;
    for (const LagObservation& observation : observations_) {
        sums[observation.endpointId] += observation.delayMs;
        counts[observation.endpointId] += 1;
    }
    QHash<QString, double> averages;
    for (auto it = sums.constBegin(); it != sums.constEnd(); ++it) {
        const int count = counts.value(it.key());
        if (count > 0) {
            averages.insert(it.key(), it.value() / static_cast<double>(count));
        }
    }
    return averages;
}

QVector<DestinationLatencySample> RuntimeLagAverager::averagedLatencySamples(qint64 nowMs) const
{
    QVector<DestinationLatencySample> samples;
    const QHash<QString, double> averages = averagedDelayMs(nowMs);
    samples.reserve(averages.size());
    for (auto it = averages.constBegin(); it != averages.constEnd(); ++it) {
        DestinationLatencySample sample;
        sample.endpointId = it.key();
        sample.inputLatencyNs = static_cast<qint64>(std::llround(it.value() * 1'000'000.0));
        samples.push_back(sample);
    }
    return samples;
}

} // namespace auralis::audio

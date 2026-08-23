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

} // namespace auralis::audio

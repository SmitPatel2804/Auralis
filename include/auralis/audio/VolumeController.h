#pragma once

#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/IPipeWireLinkBackend.h>

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace auralis::audio {

struct VolumeApplyResult {
    bool allSucceeded = true;
    QStringList failedEndpointIds;
    RouteErrorInfo error;
};

class VolumeController {
public:
    explicit VolumeController(IPipeWireLinkBackend* backend);

    static double clamp(double value);

    VolumeApplyResult setDestinationVolume(quint32 nodeId, const QString& endpointId, double value);
    VolumeApplyResult setDestinationMuted(quint32 nodeId, const QString& endpointId, bool muted);
    VolumeApplyResult setDestinationDelayMs(quint32 nodeId, const QString& endpointId, double delayMs);
    VolumeApplyResult setRouteVolume(
        const QVector<QPair<QString, quint32>>& destinations,
        double value);
    VolumeApplyResult setRouteMuted(
        const QVector<QPair<QString, quint32>>& destinations,
        bool muted);
    bool volumeSupported(quint32 nodeId) const;

private:
    IPipeWireLinkBackend* backend_ = nullptr;
};

} // namespace auralis::audio

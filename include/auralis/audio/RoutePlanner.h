#pragma once

#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/AudioSource.h>
#include <auralis/audio/PipeWireObjectStore.h>

#include <QString>
#include <QStringList>
#include <QVector>

namespace auralis::audio {

struct ResolvedPortPair {
    QString destinationId;
    quint32 outputNodeId = 0;
    quint32 outputPortId = 0;
    quint32 inputNodeId = 0;
    quint32 inputPortId = 0;
    QString channel;
};

struct ResolvedRoutePlan {
    QString sourceId;
    quint32 sourceNodeId = 0;
    QStringList destinationIds;
    QVector<ResolvedPortPair> pairs;
    RouteErrorInfo error;
};

class RoutePlanner {
public:
    ResolvedRoutePlan plan(
        const QString& sourceId,
        const QStringList& destinationEndpointIds,
        const PipeWireObjectStore& store,
        const class AudioEndpointRegistry& endpoints) const;
};

} // namespace auralis::audio

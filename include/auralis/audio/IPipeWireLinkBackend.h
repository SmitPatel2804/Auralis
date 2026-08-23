#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <optional>

namespace auralis::audio {

struct LinkCreateResult {
    quint64 ownershipToken = 0;
    quint32 globalId = 0;
};

class IPipeWireLinkBackend {
public:
    virtual ~IPipeWireLinkBackend() = default;

    virtual std::optional<LinkCreateResult> createLink(
        quint32 outputNode,
        quint32 outputPort,
        quint32 inputNode,
        quint32 inputPort,
        const QString& routeId,
        const QHash<QString, QString>& extraProps = {}) = 0;

    virtual bool destroyOwnedLink(quint64 ownershipToken) = 0;
    virtual bool destroyForeignLink(quint32 globalId) = 0;
    virtual quint32 ownedLinkGlobalId(quint64 ownershipToken) const = 0;
    virtual bool setNodeVolume(quint32 nodeId, double volume) = 0;
    virtual bool setNodeMuted(quint32 nodeId, bool muted) = 0;
    virtual bool volumeSupported(quint32 nodeId) const = 0;
    /// Software delay on a destination node. 0 clears the delay graph.
    virtual bool setNodeDelaySeconds(quint32 nodeId, double delaySeconds) = 0;

    /// PipeWire combine-stream sink that fans out to the named destinations
    /// with latency compensation. Default is unsupported.
    virtual bool ensureLatencyCompensatedFanout(const QStringList& sinkNodeNames)
    {
        Q_UNUSED(sinkNodeNames);
        return false;
    }
    virtual void destroyLatencyCompensatedFanout() {}

    /// Per-destination loopback delay line in the route path. Default is unsupported.
    virtual bool ensureDelayBridge(const QString& endpointId, const QString& destNodeName, double delaySeconds)
    {
        Q_UNUSED(endpointId);
        Q_UNUSED(destNodeName);
        Q_UNUSED(delaySeconds);
        return false;
    }
    virtual void destroyDelayBridge(const QString& endpointId) { Q_UNUSED(endpointId); }
    virtual void destroyAllDelayBridges() {}
};

} // namespace auralis::audio

#pragma once

#include <QHash>
#include <QString>

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
};

} // namespace auralis::audio

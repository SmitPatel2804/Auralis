#include <auralis/audio/LinkManager.h>

#include <auralis/core/LoggingCategories.h>

namespace auralis::audio {

LinkManager::LinkManager(IPipeWireLinkBackend* backend)
    : backend_(backend)
{
}

QVector<OwnedLink> LinkManager::createLinks(const QString& routeId, const ResolvedRoutePlan& plan, RouteErrorInfo* error)
{
    QVector<OwnedLink> created;
    if (backend_ == nullptr) {
        if (error != nullptr) {
            *error = {RouteError::InternalError, QStringLiteral("Link backend is missing")};
        }
        return created;
    }

    for (const ResolvedPortPair& pair : plan.pairs) {
        const std::optional<LinkCreateResult> result =
            backend_->createLink(pair.outputNodeId, pair.outputPortId, pair.inputNodeId, pair.inputPortId, routeId);
        if (!result.has_value() || result->ownershipToken == 0) {
            if (error != nullptr) {
                *error = {
                    RouteError::LinkCreationFailed,
                    QStringLiteral("Failed to create link %1:%2 -> %3:%4")
                        .arg(pair.outputNodeId)
                        .arg(pair.outputPortId)
                        .arg(pair.inputNodeId)
                        .arg(pair.inputPortId)};
            }
            destroyLinks(created);
            return {};
        }
        OwnedLink owned;
        owned.routeId = routeId;
        owned.destinationId = pair.destinationId;
        owned.outputNodeId = pair.outputNodeId;
        owned.outputPortId = pair.outputPortId;
        owned.inputNodeId = pair.inputNodeId;
        owned.inputPortId = pair.inputPortId;
        owned.ownershipToken = result->ownershipToken;
        owned.globalId = result->globalId;
        owned.channel = pair.channel;
        created.push_back(owned);
        qCInfo(auralisAudio) << "LinkManager owned link route=" << routeId << "token=" << owned.ownershipToken
                             << "pw=" << owned.globalId << "channel=" << owned.channel;
    }
    return created;
}

void LinkManager::destroyLinks(QVector<OwnedLink>& links)
{
    for (OwnedLink& link : links) {
        destroyLink(link);
    }
    links.clear();
}

void LinkManager::destroyLink(OwnedLink& link)
{
    if (backend_ != nullptr && link.ownershipToken != 0) {
        backend_->destroyOwnedLink(link.ownershipToken);
    }
    link.ownershipToken = 0;
    link.globalId = 0;
}

void LinkManager::forgetRuntime(QVector<OwnedLink>& links)
{
    for (OwnedLink& link : links) {
        link.ownershipToken = 0;
        link.globalId = 0;
    }
}

void LinkManager::refreshGlobalIds(QVector<OwnedLink>& links)
{
    if (backend_ == nullptr) {
        return;
    }
    for (OwnedLink& link : links) {
        if (link.ownershipToken == 0) {
            continue;
        }
        link.globalId = backend_->ownedLinkGlobalId(link.ownershipToken);
    }
}

} // namespace auralis::audio

#pragma once

#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/IPipeWireLinkBackend.h>
#include <auralis/audio/RoutePlanner.h>

#include <QString>
#include <QVector>

namespace auralis::audio {

class LinkManager {
public:
    explicit LinkManager(IPipeWireLinkBackend* backend);

    QVector<OwnedLink> createLinks(const QString& routeId, const ResolvedRoutePlan& plan, RouteErrorInfo* error);
    void destroyLinks(QVector<OwnedLink>& links);
    void destroyLink(OwnedLink& link);
    void forgetRuntime(QVector<OwnedLink>& links);

private:
    IPipeWireLinkBackend* backend_ = nullptr;
};

} // namespace auralis::audio

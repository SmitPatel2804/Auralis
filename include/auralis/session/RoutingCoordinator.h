#pragma once

#include <auralis/session/AuralisSession.h>
#include <auralis/session/SessionTypes.h>

#include <QHash>
#include <QString>

namespace auralis::audio {
class AudioEndpointRegistry;
class AudioRouter;
}

namespace auralis::bluetooth {
class DeviceRegistry;
}

namespace auralis::session {

struct MemberRouteDesired {
    QString deviceId;
    QString endpointId;
};

enum class ReconcileMode {
    /// Create/activate routes for members that are eligible (autoRestoreAllowed / Starting).
    Full,
    /// Tear down only; never create or activate new routes (Failed/Idle/Stopping).
    SuppressCreate
};

class RoutingCoordinator {
public:
    RoutingCoordinator(
        auralis::audio::AudioRouter* router,
        auralis::audio::AudioEndpointRegistry* endpoints,
        auralis::bluetooth::DeviceRegistry* devices);

    [[nodiscard]] QVector<MemberRouteDesired> desiredRoutes(const AuralisSession& session) const;
    void reconcile(
        AuralisSession& session,
        quint64 generation,
        const QHash<QString, quint64>& generations,
        ReconcileMode mode = ReconcileMode::Full);
    void stopSessionRoutes(AuralisSession& session);
    [[nodiscard]] bool sourceAvailable(const AuralisSession& session) const;
    [[nodiscard]] bool memberEndpointAvailable(const QString& deviceId) const;
    [[nodiscard]] bool memberConnected(const QString& deviceId) const;
    void refreshRuntime(AuralisSession& session);

private:
    [[nodiscard]] QString resolveEndpointId(const QString& deviceId) const;

    auralis::audio::AudioRouter* router_ = nullptr;
    auralis::audio::AudioEndpointRegistry* endpoints_ = nullptr;
    auralis::bluetooth::DeviceRegistry* devices_ = nullptr;
    QHash<QString, QString> sessionRouteOwners_;
};

} // namespace auralis::session

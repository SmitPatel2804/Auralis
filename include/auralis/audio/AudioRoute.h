#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>
#include <optional>

namespace auralis::audio {

Q_NAMESPACE

enum class RouteState {
    Inactive,
    Planning,
    Ready,
    Activating,
    Active,
    Degraded,
    Deactivating,
    Failed
};
Q_ENUM_NS(RouteState)

enum class RouteError {
    None,
    SourceNotFound,
    SourceNotRoutable,
    DestinationNotFound,
    DestinationUnavailable,
    NoCompatiblePorts,
    UnsupportedDirection,
    FormatNegotiationFailed,
    LinkCreationFailed,
    LinkEnteredErrorState,
    PipeWireDisconnected,
    PermissionDenied,
    DestinationSuspended,
    SourceRemoved,
    DestinationRemoved,
    PartialActivationFailed,
    VolumeControlUnsupported,
    VolumeControlFailed,
    InternalError
};

enum class RouteOwnerType {
    Manual,
    Session
};
Q_ENUM_NS(RouteOwnerType)

enum class RouteRecoveryPolicy {
    NoAutomaticRecovery,
    RebindOnGraphReplacement
};

enum class RouteFanoutRole {
    None,
    Leader,
    Member
};

struct RouteErrorInfo {
    RouteError category = RouteError::None;
    QString detail;

    bool hasError() const noexcept { return category != RouteError::None; }
};

struct OwnedLink {
    QString routeId;
    QString destinationId;
    quint32 outputNodeId = 0;
    quint32 outputPortId = 0;
    quint32 inputNodeId = 0;
    quint32 inputPortId = 0;
    quint64 ownershipToken = 0;
    quint32 globalId = 0;
    QString channel;
};

struct AudioRoute {
    QString id;
    QString sourceId;
    QStringList destinationIds;
    bool enabled = false;
    RouteState state = RouteState::Inactive;
    RouteErrorInfo error;
    QDateTime createdAt;
    QDateTime activatedAt;
    QVector<OwnedLink> ownedLinks;
    double volume = 1.0;
    bool muted = false;
    RouteRecoveryPolicy recoveryPolicy = RouteRecoveryPolicy::RebindOnGraphReplacement;
    RouteOwnerType ownerType = RouteOwnerType::Manual;
    QString ownerId;
    RouteFanoutRole fanoutRole = RouteFanoutRole::None;
};

QString toString(RouteState state);
QString toString(RouteError error);
QString toString(RouteOwnerType ownerType);

} // namespace auralis::audio

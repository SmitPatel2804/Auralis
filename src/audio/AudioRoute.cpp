#include <auralis/audio/AudioRoute.h>

namespace auralis::audio {

QString toString(RouteState state)
{
    switch (state) {
    case RouteState::Inactive:
        return QStringLiteral("Inactive");
    case RouteState::Planning:
        return QStringLiteral("Planning");
    case RouteState::Ready:
        return QStringLiteral("Ready");
    case RouteState::Activating:
        return QStringLiteral("Activating");
    case RouteState::Active:
        return QStringLiteral("Active");
    case RouteState::Degraded:
        return QStringLiteral("Degraded");
    case RouteState::Deactivating:
        return QStringLiteral("Deactivating");
    case RouteState::Failed:
        return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

QString toString(RouteError error)
{
    switch (error) {
    case RouteError::None:
        return QStringLiteral("None");
    case RouteError::SourceNotFound:
        return QStringLiteral("SourceNotFound");
    case RouteError::SourceNotRoutable:
        return QStringLiteral("SourceNotRoutable");
    case RouteError::DestinationNotFound:
        return QStringLiteral("DestinationNotFound");
    case RouteError::DestinationUnavailable:
        return QStringLiteral("DestinationUnavailable");
    case RouteError::NoCompatiblePorts:
        return QStringLiteral("NoCompatiblePorts");
    case RouteError::UnsupportedDirection:
        return QStringLiteral("UnsupportedDirection");
    case RouteError::FormatNegotiationFailed:
        return QStringLiteral("FormatNegotiationFailed");
    case RouteError::LinkCreationFailed:
        return QStringLiteral("LinkCreationFailed");
    case RouteError::LinkEnteredErrorState:
        return QStringLiteral("LinkEnteredErrorState");
    case RouteError::PipeWireDisconnected:
        return QStringLiteral("PipeWireDisconnected");
    case RouteError::PermissionDenied:
        return QStringLiteral("PermissionDenied");
    case RouteError::DestinationSuspended:
        return QStringLiteral("DestinationSuspended");
    case RouteError::SourceRemoved:
        return QStringLiteral("SourceRemoved");
    case RouteError::DestinationRemoved:
        return QStringLiteral("DestinationRemoved");
    case RouteError::PartialActivationFailed:
        return QStringLiteral("PartialActivationFailed");
    case RouteError::VolumeControlUnsupported:
        return QStringLiteral("VolumeControlUnsupported");
    case RouteError::VolumeControlFailed:
        return QStringLiteral("VolumeControlFailed");
    case RouteError::InternalError:
        return QStringLiteral("InternalError");
    }
    return QStringLiteral("InternalError");
}

QString toString(RouteOwnerType ownerType)
{
    switch (ownerType) {
    case RouteOwnerType::Manual:
        return QStringLiteral("Manual");
    case RouteOwnerType::Session:
        return QStringLiteral("Session");
    }
    return QStringLiteral("Manual");
}

} // namespace auralis::audio

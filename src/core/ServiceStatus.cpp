#include <auralis/core/ServiceStatus.h>

namespace auralis::core {

QString toString(ServiceStatus status)
{
    switch (status) {
    case ServiceStatus::Uninitialized:
        return QStringLiteral("Uninitialized");
    case ServiceStatus::Initializing:
        return QStringLiteral("Initializing");
    case ServiceStatus::Ready:
        return QStringLiteral("Ready");
    case ServiceStatus::Error:
        return QStringLiteral("Error");
    }

    return QStringLiteral("Unknown");
}

} // namespace auralis::core

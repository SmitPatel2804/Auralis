#pragma once

#include <QString>

namespace auralis::core {

enum class ServiceStatus {
    Uninitialized,
    Initializing,
    Ready,
    Error
};

QString toString(ServiceStatus status);

} // namespace auralis::core

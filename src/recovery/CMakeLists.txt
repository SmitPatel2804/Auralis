add_library(auralis-recovery
    RecoveryManager.cpp
    RecoveryTypes.cpp
    ServiceRetryPolicy.cpp
    SystemPowerMonitor.cpp
    "${CMAKE_SOURCE_DIR}/include/auralis/recovery/RecoveryManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/recovery/RecoveryTypes.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/recovery/ServiceRetryPolicy.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/recovery/SystemPowerMonitor.h"
)

add_library(Auralis::Recovery ALIAS auralis-recovery)

target_include_directories(auralis-recovery
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(auralis-recovery
    PUBLIC
        Qt6::Core
        Qt6::DBus
)

auralis_apply_warnings(auralis-recovery)

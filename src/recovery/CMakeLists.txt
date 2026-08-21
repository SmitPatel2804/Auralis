set(AURALIS_POWER_MONITOR_SOURCE SystemPowerMonitor.cpp)
if(WIN32)
    set(AURALIS_POWER_MONITOR_SOURCE SystemPowerMonitor_windows.cpp)
elseif(APPLE)
    set(AURALIS_POWER_MONITOR_SOURCE SystemPowerMonitor_macos.mm)
endif()

add_library(auralis-recovery
    RecoveryManager.cpp
    RecoveryTypes.cpp
    ServiceRetryPolicy.cpp
    ${AURALIS_POWER_MONITOR_SOURCE}
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
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(auralis-recovery PUBLIC Qt6::DBus)
elseif(APPLE)
    target_link_libraries(auralis-recovery PRIVATE "-framework AppKit")
endif()

auralis_apply_warnings(auralis-recovery)

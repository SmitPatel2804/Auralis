add_library(auralis-core
    ApplicationCore.cpp
    ConfigurationManager.cpp
    Logger.cpp
    LoggingCategories.cpp
    ServiceStatus.cpp
    "${CMAKE_SOURCE_DIR}/src/ui/NotificationController.cpp"
    "${CMAKE_SOURCE_DIR}/src/ui/DiagnosticsLogModel.cpp"
    "${CMAKE_SOURCE_DIR}/include/auralis/core/ApplicationCore.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/core/ConfigurationManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/ui/NotificationController.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/ui/DiagnosticsLogModel.h"
)

add_library(Auralis::Core ALIAS auralis-core)

target_include_directories(auralis-core
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(auralis-core
    PUBLIC
        auralis-recovery
        Qt6::Core
        Qt6::Gui
)

target_compile_definitions(auralis-core
    PUBLIC
        $<$<BOOL:${AURALIS_ENABLE_FILE_LOGGING}>:AURALIS_ENABLE_FILE_LOGGING>
)

auralis_apply_warnings(auralis-core)

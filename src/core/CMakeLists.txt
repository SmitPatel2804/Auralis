add_library(auralis-core
    ApplicationCore.cpp
    ConfigurationManager.cpp
    Logger.cpp
    LoggingCategories.cpp
    ServiceStatus.cpp
    "${CMAKE_SOURCE_DIR}/include/auralis/core/ApplicationCore.h"
)

add_library(Auralis::Core ALIAS auralis-core)

target_include_directories(auralis-core
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(auralis-core
    PUBLIC
        Qt6::Core
)

target_compile_definitions(auralis-core
    PUBLIC
        $<$<BOOL:${AURALIS_ENABLE_FILE_LOGGING}>:AURALIS_ENABLE_FILE_LOGGING>
)

auralis_apply_warnings(auralis-core)

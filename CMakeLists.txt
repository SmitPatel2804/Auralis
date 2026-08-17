cmake_minimum_required(VERSION 3.25)

project(Auralis
    VERSION 0.1.0
    DESCRIPTION "Multi-hearing-device audio hub for Linux"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(AuralisOptions)
include(AuralisCompilerWarnings)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick Test)

qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/bluetooth)
add_subdirectory(src/audio)
add_subdirectory(src/devices)
add_subdirectory(src/session)
add_subdirectory(ui)
add_subdirectory(apps/desktop)

if(AURALIS_BUILD_TESTS)
    add_subdirectory(tests)
endif()

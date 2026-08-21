cmake_minimum_required(VERSION 3.25)

project(Auralis
    VERSION 0.1.0
    DESCRIPTION "Cross-platform multi-hearing-device audio hub"
    LANGUAGES CXX
)

# Qt 6 removed its Win32 Bluetooth implementation. The remaining Windows
# backend is WinRT-based and is not built for MinGW, where Qt silently falls
# back to a non-functional dummy backend. Refuse that configuration so a
# successful build cannot accidentally ship without Bluetooth support.
if(WIN32 AND NOT MSVC)
    message(FATAL_ERROR
        "Auralis Windows builds require MSVC 2022. Qt Bluetooth has no "
        "functional Windows backend when Qt is built with MinGW. Install the "
        "Qt MSVC 2022 64-bit kit with Qt Connectivity and Qt Multimedia.")
endif()

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(AuralisOptions)
include(AuralisCompilerWarnings)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick Test)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt6 6.8 REQUIRED COMPONENTS DBus)
else()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Bluetooth Multimedia)
endif()

qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

add_subdirectory(src/recovery)
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

include(AuralisPackaging)

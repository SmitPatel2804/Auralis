find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

add_library(auralis-audio
    AudioEndpoint.cpp
    AudioEndpointClassifier.cpp
    AudioEndpointListModel.cpp
    AudioEndpointRegistry.cpp
    AudioRoute.cpp
    AudioRouter.cpp
    AudioSource.cpp
    AudioSourceListModel.cpp
    EndpointResolver.cpp
    LinkManager.cpp
    PipeWireConnection.cpp
    PipeWireManager.cpp
    PipeWireObjectStore.cpp
    PipeWireProperties.cpp
    PipeWireTypes.cpp
    RoutePlanner.cpp
    RouteListModel.cpp
    VolumeController.cpp
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioEndpoint.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioEndpointClassifier.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioEndpointListModel.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioEndpointRegistry.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioRoute.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioRouter.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioSource.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioSourceListModel.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/EndpointResolver.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/IPipeWireLinkBackend.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/IPipeWireManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/LinkManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireConnection.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireObjectStore.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireProperties.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireTypes.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/RoutePlanner.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/RouteListModel.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/VolumeController.h"
)

add_library(Auralis::Audio ALIAS auralis-audio)

target_include_directories(auralis-audio
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(auralis-audio
    PUBLIC
        auralis-core
        auralis-bluetooth
        Qt6::Core
        PkgConfig::PIPEWIRE
)

auralis_apply_warnings(auralis-audio)

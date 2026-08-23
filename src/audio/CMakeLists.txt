set(AURALIS_AUDIO_SOURCES
    AudioEndpoint.cpp
    AudioEndpointClassifier.cpp
    AudioEndpointListModel.cpp
    AudioEndpointRegistry.cpp
    AudioRoute.cpp
    AudioRouter.cpp
    AudioSource.cpp
    AudioSourceListModel.cpp
    DestinationSync.cpp
    AcousticLagCalibrator.cpp
    EndpointResolver.cpp
    LinkManager.cpp
    PipeWireObjectStore.cpp
    PipeWireProperties.cpp
    PipeWireTypes.cpp
    PipeWireVirtualOutput.cpp
    RoutePlanner.cpp
    RouteListModel.cpp
    VolumeController.cpp
    VirtualAudioDevice.cpp
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioEndpoint.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioEndpointClassifier.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioEndpointListModel.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioEndpointRegistry.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioRoute.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioRouter.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioSource.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AudioSourceListModel.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/DestinationSync.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/AcousticLagCalibrator.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/EndpointResolver.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/IAudioManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/IPipeWireLinkBackend.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/IPipeWireManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/LinkManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireObjectStore.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireProperties.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireTypes.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireVirtualOutput.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/RoutePlanner.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/RouteListModel.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/VolumeController.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/audio/VirtualAudioDevice.h"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND AURALIS_AUDIO_SOURCES
        PipeWireConnection.cpp
        PipeWireManager.cpp
        "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireConnection.h"
        "${CMAKE_SOURCE_DIR}/include/auralis/audio/PipeWireManager.h"
    )
else()
    list(APPEND AURALIS_AUDIO_SOURCES
        NativeAudioManager.cpp
        "${CMAKE_SOURCE_DIR}/include/auralis/audio/NativeAudioManager.h"
    )
    if(WIN32)
        list(APPEND AURALIS_AUDIO_SOURCES
            WindowsAudioSessions.cpp
            WindowsAudioSessions.h
            WindowsAudioRenderSink.cpp
            WindowsAudioRenderSink.h
            WindowsEndpointLoopbackCapture.cpp
            WindowsEndpointLoopbackCapture.h
            WindowsProcessLoopbackCapture.cpp
            WindowsProcessLoopbackCapture.h
        )
    endif()
endif()

add_library(auralis-audio ${AURALIS_AUDIO_SOURCES})

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
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET "libpipewire-0.3>=0.3.60")
    target_link_libraries(auralis-audio PUBLIC PkgConfig::PIPEWIRE)
else()
    target_link_libraries(auralis-audio PUBLIC Qt6::Multimedia)
    if(WIN32)
        target_link_libraries(auralis-audio PRIVATE Ole32 Mmdevapi)
        target_compile_definitions(auralis-audio PRIVATE _WIN32_WINNT=0x0A00)
    endif()
endif()

auralis_apply_warnings(auralis-audio)

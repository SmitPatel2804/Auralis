if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    auralis_add_test(tst_PipeWireManager tst_PipeWireManager.cpp)
    target_link_libraries(tst_PipeWireManager PRIVATE auralis-audio)
else()
    auralis_add_test(tst_NativeAudioManager tst_NativeAudioManager.cpp)
    target_link_libraries(tst_NativeAudioManager PRIVATE auralis-audio auralis-bluetooth)
    if(WIN32)
        add_executable(auralis-audio-fixture windows_audio_fixture.cpp)
        target_link_libraries(auralis-audio-fixture PRIVATE Qt6::Core Qt6::Multimedia)
        auralis_apply_warnings(auralis-audio-fixture)
        auralis_add_test(tst_WindowsProcessLoopback tst_WindowsProcessLoopback.cpp)
        target_link_libraries(tst_WindowsProcessLoopback PRIVATE auralis-audio)
        target_include_directories(tst_WindowsProcessLoopback PRIVATE "${CMAKE_SOURCE_DIR}/src/audio")
        target_compile_definitions(
            tst_WindowsProcessLoopback
            PRIVATE AURALIS_AUDIO_FIXTURE_PATH="$<TARGET_FILE:auralis-audio-fixture>")
        add_dependencies(tst_WindowsProcessLoopback auralis-audio-fixture)
        auralis_add_test(tst_WindowsEndpointLoopback tst_WindowsEndpointLoopback.cpp)
        target_link_libraries(tst_WindowsEndpointLoopback PRIVATE auralis-audio Qt6::Multimedia)
        target_include_directories(tst_WindowsEndpointLoopback PRIVATE "${CMAKE_SOURCE_DIR}/src/audio")
        target_compile_definitions(
            tst_WindowsEndpointLoopback
            PRIVATE AURALIS_AUDIO_FIXTURE_PATH="$<TARGET_FILE:auralis-audio-fixture>")
        add_dependencies(tst_WindowsEndpointLoopback auralis-audio-fixture)
        auralis_add_test(tst_WindowsAudioRenderSink tst_WindowsAudioRenderSink.cpp)
        target_link_libraries(tst_WindowsAudioRenderSink PRIVATE auralis-audio Qt6::Multimedia)
        target_include_directories(tst_WindowsAudioRenderSink PRIVATE "${CMAKE_SOURCE_DIR}/src/audio")
    endif()
endif()

auralis_add_test(tst_VirtualAudioDevice tst_VirtualAudioDevice.cpp)
target_link_libraries(tst_VirtualAudioDevice PRIVATE auralis-audio)

auralis_add_test(tst_PipeWireProperties tst_PipeWireProperties.cpp)
target_link_libraries(tst_PipeWireProperties PRIVATE auralis-audio)

auralis_add_test(tst_AudioEndpoints tst_AudioEndpoints.cpp)
target_link_libraries(tst_AudioEndpoints PRIVATE auralis-audio)
target_include_directories(tst_AudioEndpoints PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

auralis_add_test(tst_EndpointResolver tst_EndpointResolver.cpp)
target_link_libraries(tst_EndpointResolver PRIVATE auralis-audio)
target_include_directories(tst_EndpointResolver PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

auralis_add_test(tst_AudioRoute tst_AudioRoute.cpp)
target_link_libraries(tst_AudioRoute PRIVATE auralis-audio)

auralis_add_test(tst_AudioSources tst_AudioSources.cpp)
target_link_libraries(tst_AudioSources PRIVATE auralis-audio)
target_include_directories(tst_AudioSources PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

auralis_add_test(tst_RoutePlanner tst_RoutePlanner.cpp)
target_link_libraries(tst_RoutePlanner PRIVATE auralis-audio)
target_include_directories(tst_RoutePlanner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

auralis_add_test(tst_AudioRouter tst_AudioRouter.cpp)
target_link_libraries(tst_AudioRouter PRIVATE auralis-audio)
target_include_directories(tst_AudioRouter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

auralis_add_test(tst_VolumeController tst_VolumeController.cpp)
target_link_libraries(tst_VolumeController PRIVATE auralis-audio)
target_include_directories(tst_VolumeController PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

auralis_add_test(tst_RouteListModel tst_RouteListModel.cpp)
target_link_libraries(tst_RouteListModel PRIVATE auralis-audio auralis-bluetooth)
target_include_directories(tst_RouteListModel PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

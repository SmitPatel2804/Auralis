if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    auralis_add_test(tst_DesktopBackendSmoke tst_DesktopBackendSmoke.cpp "${CMAKE_SOURCE_DIR}/src/core/QmlTypeRegistration.cpp")
    target_link_libraries(tst_DesktopBackendSmoke
        PRIVATE
            auralis-core
            auralis-bluetooth
            auralis-audio
            auralis-devices
            auralis-session
            auralis-ui
            auralis-uiplugin
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
    )
    target_include_directories(tst_DesktopBackendSmoke PRIVATE "${CMAKE_SOURCE_DIR}/tests/unit/bluetooth")
    qt_import_qml_plugins(tst_DesktopBackendSmoke)
    set_tests_properties(tst_DesktopBackendSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

auralis_add_test(tst_QmlComponents tst_QmlComponents.cpp "${CMAKE_SOURCE_DIR}/src/core/QmlTypeRegistration.cpp")
target_link_libraries(tst_QmlComponents
    PRIVATE
        auralis-core
        auralis-bluetooth
        auralis-audio
        auralis-devices
        auralis-session
        auralis-ui
        auralis-uiplugin
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)
qt_import_qml_plugins(tst_QmlComponents)
set_tests_properties(tst_QmlComponents PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    auralis_add_test(tst_BlueZLiveIntegration tst_BlueZLiveIntegration.cpp)
    target_link_libraries(tst_BlueZLiveIntegration PRIVATE auralis-bluetooth)

    auralis_add_test(tst_PipeWireLiveIntegration tst_PipeWireLiveIntegration.cpp)
    target_link_libraries(tst_PipeWireLiveIntegration PRIVATE auralis-audio auralis-bluetooth)

    auralis_add_test(tst_AudioRoutingLiveIntegration tst_AudioRoutingLiveIntegration.cpp)
    target_link_libraries(tst_AudioRoutingLiveIntegration PRIVATE auralis-audio auralis-bluetooth)
endif()

auralis_add_test(tst_SessionLifecycleIntegration tst_SessionLifecycleIntegration.cpp)
target_link_libraries(tst_SessionLifecycleIntegration PRIVATE auralis-session auralis-audio auralis-bluetooth)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    auralis_add_test(tst_SessionLiveIntegration tst_SessionLiveIntegration.cpp)
    target_link_libraries(tst_SessionLiveIntegration PRIVATE auralis-session auralis-audio auralis-bluetooth)

    auralis_add_test(tst_Phase7HardwareLive tst_Phase7HardwareLive.cpp)
    target_link_libraries(tst_Phase7HardwareLive PRIVATE auralis-session auralis-audio auralis-bluetooth)

    add_executable(auralis-play-both play_both_live.cpp)
    target_link_libraries(auralis-play-both PRIVATE auralis-session auralis-audio auralis-bluetooth Qt6::Core)
    auralis_apply_warnings(auralis-play-both)

    auralis_add_test(tst_SessionManagedReconnectIntegration tst_SessionManagedReconnectIntegration.cpp)
    target_link_libraries(tst_SessionManagedReconnectIntegration PRIVATE auralis-session auralis-audio auralis-bluetooth)
    target_include_directories(tst_SessionManagedReconnectIntegration PRIVATE
        "${CMAKE_SOURCE_DIR}/tests/unit/bluetooth"
        "${CMAKE_SOURCE_DIR}/tests/unit/audio"
    )
endif()

auralis_add_test(tst_ServiceRecoveryIntegration tst_ServiceRecoveryIntegration.cpp)
target_link_libraries(tst_ServiceRecoveryIntegration PRIVATE auralis-recovery auralis-core)
set_tests_properties(tst_ServiceRecoveryIntegration PROPERTIES LABELS "integration;recovery")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    auralis_add_test(tst_Phase8RecoveryHarness tst_Phase8RecoveryHarness.cpp)
    target_link_libraries(tst_Phase8RecoveryHarness PRIVATE auralis-recovery auralis-core auralis-bluetooth auralis-audio auralis-session)
    target_include_directories(tst_Phase8RecoveryHarness PRIVATE
        "${CMAKE_SOURCE_DIR}/tests/unit/bluetooth"
        "${CMAKE_SOURCE_DIR}/tests/unit/audio"
    )
    set_tests_properties(tst_Phase8RecoveryHarness PROPERTIES LABELS "integration;recovery")

    auralis_add_test(tst_Phase8Stress tst_Phase8Stress.cpp)
    target_link_libraries(tst_Phase8Stress PRIVATE auralis-recovery auralis-core auralis-bluetooth auralis-audio auralis-session)
    set_tests_properties(tst_Phase8Stress PROPERTIES LABELS "stress;recovery")
endif()

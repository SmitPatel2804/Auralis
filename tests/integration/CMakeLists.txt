auralis_add_test(tst_DesktopBackendSmoke tst_DesktopBackendSmoke.cpp)
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

set_tests_properties(tst_DesktopBackendSmoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

auralis_add_test(tst_BlueZLiveIntegration tst_BlueZLiveIntegration.cpp)
target_link_libraries(tst_BlueZLiveIntegration PRIVATE auralis-bluetooth)

auralis_add_test(tst_PipeWireLiveIntegration tst_PipeWireLiveIntegration.cpp)
target_link_libraries(tst_PipeWireLiveIntegration PRIVATE auralis-audio auralis-bluetooth)

auralis_add_test(tst_AudioRoutingLiveIntegration tst_AudioRoutingLiveIntegration.cpp)
target_link_libraries(tst_AudioRoutingLiveIntegration PRIVATE auralis-audio auralis-bluetooth)


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

qt_import_qml_plugins(tst_DesktopBackendSmoke)

set_tests_properties(tst_DesktopBackendSmoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

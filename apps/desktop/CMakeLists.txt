qt_add_executable(auralis-desktop
    main.cpp
    DesktopApplication.cpp
    DesktopApplication.h
)

target_link_libraries(auralis-desktop
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

set_target_properties(auralis-desktop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps/desktop"
    OUTPUT_NAME "auralis-desktop"
)

qt_import_qml_plugins(auralis-desktop)

auralis_apply_warnings(auralis-desktop)

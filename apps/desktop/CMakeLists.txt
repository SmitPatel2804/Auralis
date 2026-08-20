qt_add_executable(auralis-desktop
    main.cpp
    DesktopApplication.cpp
    DesktopApplication.h
    "${CMAKE_SOURCE_DIR}/src/core/QmlTypeRegistration.cpp"
)

target_link_libraries(auralis-desktop
    PRIVATE
        auralis-core
        auralis-bluetooth
        auralis-audio
        auralis-devices
        auralis-session
        auralis-recovery
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

include(GNUInstallDirs)

install(TARGETS auralis-desktop
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    "${CMAKE_SOURCE_DIR}/data/io.github.auralis.Auralis.desktop"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
)

install(FILES
    "${CMAKE_SOURCE_DIR}/data/io.github.auralis.Auralis.metainfo.xml"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo"
)

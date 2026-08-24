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

if(WIN32)
    set_target_properties(auralis-desktop PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    set_target_properties(auralis-desktop PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Auralis"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.auralis.Auralis"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/AuralisInfo.plist.in"
    )
endif()

# The application QML lives in the linked auralis-ui static module rather than
# beside this executable target. Tell Qt's deployment scanner where those QML
# sources are so packaged builds include transitive imports such as QtQuick,
# QtQuick.Controls, and QtQuick.Layouts on every desktop platform.
set_property(TARGET auralis-desktop PROPERTY
    QT_QML_IMPORT_SCANNER_EXTRA_ROOT_PATHS "${CMAKE_SOURCE_DIR}/ui"
)

qt_import_qml_plugins(auralis-desktop)

auralis_apply_warnings(auralis-desktop)

include(GNUInstallDirs)

install(TARGETS auralis-desktop
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES
        "${CMAKE_SOURCE_DIR}/data/io.github.auralis.Auralis.desktop"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
    )

    install(FILES
        "${CMAKE_SOURCE_DIR}/data/io.github.auralis.Auralis.metainfo.xml"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo"
    )

    install(FILES
        "${CMAKE_SOURCE_DIR}/data/icons/hicolor/scalable/apps/io.github.auralis.Auralis.svg"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps"
    )

    install(FILES
        "${CMAKE_SOURCE_DIR}/data/pipewire/90-auralis-virtual-output.conf"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/pipewire/pipewire.conf.d"
    )
endif()

install(FILES
    "${CMAKE_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/auralis"
)

set(AURALIS_LINUX_BUNDLE_QT ON CACHE BOOL "Bundle Qt into Linux packages")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Distro QML plugins often have no DT_RPATH. CMake 3.21+ file(RPATH_SET)
    # then fails. patchelf can add an RPATH; without it, do not bundle Qt and
    # declare Debian Qt/QML dependencies instead. Official aqt/CI trees still
    # bundle because those plugins already carry an RPATH.
    find_program(AURALIS_PATCHELF_EXECUTABLE patchelf)
    if(AURALIS_PATCHELF_EXECUTABLE)
        set(QT_DEPLOY_USE_PATCHELF ON)
        set(QT_DEPLOY_PATCHELF_EXECUTABLE "${AURALIS_PATCHELF_EXECUTABLE}")
    elseif(Qt6_DIR MATCHES "^/usr/")
        set(AURALIS_LINUX_BUNDLE_QT OFF CACHE BOOL "Bundle Qt into Linux packages" FORCE)
        message(STATUS
            "Linux packaging will not bundle Qt (system Qt; install patchelf to enable RPATH rewriting).")
    endif()

    if(AURALIS_LINUX_BUNDLE_QT)
        # qt_generate_deploy_qml_app_script does not accept explicit QML/PLUGIN
        # install dirs. For aqt trees those defaults can be absolute, which makes
        # file(INSTALL) drop QML outside CPack's usr/ staging (verify then finds
        # libQt6Core in usr/lib but no /qml/QtQuick/). Force prefix-relative dirs
        # and keep dependency resolution inside the bundled Qt prefix on CI.
        set(auralis_linux_deploy_dirs "")
        set(auralis_grd_filter_line "")
        if(NOT Qt6_DIR MATCHES "^/usr/")
            get_filename_component(AURALIS_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
            string(REGEX REPLACE "([][+.*()^$])" "\\\\\\1" AURALIS_QT_PREFIX_REGEX "${AURALIS_QT_PREFIX}")
            set(QT_DEPLOY_QML_DIR "qml")
            set(QT_DEPLOY_PLUGINS_DIR "plugins")
            set(auralis_linux_deploy_dirs
                "QML_DIR qml"
                "PLUGINS_DIR plugins")
            set(auralis_grd_filter_line
                "PRE_INCLUDE_REGEXES \"${AURALIS_QT_PREFIX_REGEX}\"")
        endif()
        set(auralis_linux_executable "\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:auralis-desktop>")
        set(auralis_linux_deploy_dir_args "")
        foreach(auralis_linux_deploy_dir IN LISTS auralis_linux_deploy_dirs)
            string(APPEND auralis_linux_deploy_dir_args "                    ${auralis_linux_deploy_dir}\n")
        endforeach()
        if(NOT Qt6_DIR MATCHES "^/usr/")
            set(auralis_qml_assets_dir "qml")
        else()
            set(auralis_qml_assets_dir "\${QT_DEPLOY_QML_DIR}")
        endif()
        # Build CONTENT in one string. Embedding a CMake list such as
        # PRE_INCLUDE_REGEXES;<path> directly in the qt_generate_deploy_script()
        # call splits on ';' and leaks <path> as an unexpected argument.
        set(auralis_deploy_content "
include(\"\${CMAKE_CURRENT_LIST_DIR}/../../cmake/AuralisLinuxQtDeploy.cmake\")
qt_deploy_qml_imports(
    TARGET auralis-desktop
${auralis_linux_deploy_dir_args}    PLUGINS_FOUND auralis_qml_plugins
)
auralis_deploy_bundled_qml_assets(auralis-desktop ${auralis_qml_assets_dir})
qt_deploy_runtime_dependencies(
    EXECUTABLE \"${auralis_linux_executable}\"
    ADDITIONAL_MODULES \${auralis_qml_plugins}
    GENERATE_QT_CONF
${auralis_linux_deploy_dir_args}    ${auralis_grd_filter_line}
)
")
        qt_generate_deploy_script(
            TARGET auralis-desktop
            OUTPUT_SCRIPT auralis_deploy_script
            CONTENT "${auralis_deploy_content}"
        )
    endif()
else()
    set(AURALIS_DEPLOY_OPTIONS
        NO_UNSUPPORTED_PLATFORM_ERROR
        DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
    )
    if(MSVC)
        # The ZIP carries app-local MSVC runtime DLLs via
        # InstallRequiredSystemLibraries. Do not add vc_redist.x64.exe: merely
        # placing the installer beside the application does not make a portable
        # extracted build runnable on a clean machine.
        list(APPEND AURALIS_DEPLOY_OPTIONS NO_COMPILER_RUNTIME)
    endif()
    if(APPLE)
        list(APPEND AURALIS_DEPLOY_OPTIONS MACOS_BUNDLE_POST_BUILD)
    endif()

    qt_generate_deploy_qml_app_script(
        TARGET auralis-desktop
        OUTPUT_SCRIPT auralis_deploy_script
        ${AURALIS_DEPLOY_OPTIONS}
    )
endif()
if(DEFINED auralis_deploy_script)
    install(SCRIPT ${auralis_deploy_script})
endif()

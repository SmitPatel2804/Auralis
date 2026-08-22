include(GNUInstallDirs)

if(MSVC)
    # Make ZIP packages runnable without a machine-wide Visual C++
    # Redistributable installation. These DLLs are resolved from the selected
    # MSVC toolchain at configure time and installed next to the executable.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    include(InstallRequiredSystemLibraries)
endif()

set(CPACK_PACKAGE_NAME "auralis")
set(CPACK_PACKAGE_VENDOR "Auralis")
# No authoritative public contact in-tree yet — local builds remain functional.
# PUBLIC PACKAGE RELEASE METADATA: PENDING OWNER CONTACT
set(CPACK_PACKAGE_CONTACT "Auralis (contact pending)")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform multi-device Bluetooth audio hub")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_SOURCE_GENERATOR "")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # DEB is the native reference package. TGZ keeps the exact install tree
    # available to other distro packagers without pretending to own their
    # dependency metadata.
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Auralis (contact pending)")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "pipewire (>= 0.3.60), libpipewire-0.3-modules (>= 0.3.60), wireplumber, bluez")
    # dpkg-shlibdeps needs Debian shlibs metadata. Official aqt/CI Qt trees
    # are not packaged that way, so automatic dependency scanning fails there.
    if(DEFINED ENV{CI})
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    else()
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    endif()
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
elseif(WIN32)
    find_program(AURALIS_MAKENSIS_EXECUTABLE makensis)
    if(AURALIS_MAKENSIS_EXECUTABLE)
        set(CPACK_GENERATOR "NSIS;ZIP")
        set(CPACK_NSIS_DISPLAY_NAME "Auralis")
        set(CPACK_NSIS_PACKAGE_NAME "Auralis")
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    else()
        set(CPACK_GENERATOR "ZIP")
    endif()
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)

# Installed by apps/desktop/CMakeLists.txt for bundled Linux packages only.
# qt_deploy_qml_imports copies each module's qmldir and plugin .so, but runtime
# also needs the module's .qml files (Controls styles, etc.). Windows/macOS
# deploy tools copy those automatically; on Linux we mirror the import scanner
# paths into the package prefix.

function(auralis_deploy_bundled_qml_assets target qml_dir)
    if("${target}" STREQUAL "")
        message(FATAL_ERROR "auralis_deploy_bundled_qml_assets: target name must not be empty")
    endif()
    if("${qml_dir}" STREQUAL "")
        message(FATAL_ERROR "auralis_deploy_bundled_qml_assets: QML_DIR must not be empty")
    endif()

    set(imports_file "${__QT_DEPLOY_IMPL_DIR}/../apps/desktop/.qt/qml_imports/${target}_build.cmake")
    if(NOT EXISTS "${imports_file}")
        message(FATAL_ERROR
            "Missing QML import scanner output for ${target}: ${imports_file}")
    endif()
    include("${imports_file}")

    if(NOT qml_import_scanner_imports_count GREATER 0)
        return()
    endif()

    set(deployed_paths "")
    math(EXPR last_index "${qml_import_scanner_imports_count} - 1")
    foreach(index RANGE 0 ${last_index})
        cmake_parse_arguments(entry
            ""
            "CLASSNAME;NAME;PATH;PLUGIN;RELATIVEPATH;TYPE;VERSION;LINKTARGET"
            "COMPONENTS"
            ${qml_import_scanner_import_${index}}
        )
        if("${entry_PATH}" STREQUAL "" OR "${entry_RELATIVEPATH}" STREQUAL "")
            continue()
        endif()
        if("${entry_PATH}" IN_LIST deployed_paths)
            continue()
        endif()
        list(APPEND deployed_paths "${entry_PATH}")

        set(relative_qmldir "${qml_dir}/${entry_RELATIVEPATH}")
        if("${CMAKE_INSTALL_PREFIX}" STREQUAL "")
            set(install_root "$ENV{DESTDIR}./${relative_qmldir}")
        else()
            set(install_root "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${relative_qmldir}")
        endif()

        # file(INSTALL DIRECTORY ...) is unreliable under CMake 4 script mode;
        # file(COPY) with an explicit DESTDIR-aware destination works on CI and
        # local installs alike.
        file(COPY "${entry_PATH}/"
            DESTINATION "${install_root}"
            USE_SOURCE_PERMISSIONS
        )
    endforeach()
endfunction()

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(components/StatusRow.qml PROPERTIES QT_RESOURCE_ALIAS StatusRow.qml)
set_source_files_properties(components/DeviceRow.qml PROPERTIES QT_RESOURCE_ALIAS DeviceRow.qml)
set_source_files_properties(components/EndpointRow.qml PROPERTIES QT_RESOURCE_ALIAS EndpointRow.qml)

qt_add_library(auralis-ui STATIC)

qt_add_qml_module(auralis-ui
    URI Auralis.Ui
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Auralis/Ui"
    QML_FILES
        qml/Main.qml
        components/StatusRow.qml
        components/DeviceRow.qml
        components/PairingPrompt.qml
        components/EndpointRow.qml
)

add_library(Auralis::Ui ALIAS auralis-ui)

target_link_libraries(auralis-ui
    PUBLIC
        Qt6::Quick
        Qt6::Qml
)

auralis_apply_warnings(auralis-ui)

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/AppShell.qml PROPERTIES QT_RESOURCE_ALIAS AppShell.qml)
set_source_files_properties(qml/theme/Theme.qml PROPERTIES
    QT_RESOURCE_ALIAS Theme.qml
    QT_QML_SINGLETON_TYPE TRUE
)
set_source_files_properties(qml/theme/Metrics.qml PROPERTIES
    QT_RESOURCE_ALIAS Metrics.qml
    QT_QML_SINGLETON_TYPE TRUE
)
set_source_files_properties(qml/navigation/NavigationRail.qml PROPERTIES QT_RESOURCE_ALIAS NavigationRail.qml)
set_source_files_properties(qml/components/StatusBadge.qml PROPERTIES QT_RESOURCE_ALIAS StatusBadge.qml)
set_source_files_properties(qml/components/SectionCard.qml PROPERTIES QT_RESOURCE_ALIAS SectionCard.qml)
set_source_files_properties(qml/components/EmptyState.qml PROPERTIES QT_RESOURCE_ALIAS EmptyState.qml)
set_source_files_properties(qml/components/ErrorBanner.qml PROPERTIES QT_RESOURCE_ALIAS ErrorBanner.qml)
set_source_files_properties(qml/components/ConfirmDialog.qml PROPERTIES QT_RESOURCE_ALIAS ConfirmDialog.qml)
set_source_files_properties(qml/components/ToastHost.qml PROPERTIES QT_RESOURCE_ALIAS ToastHost.qml)
set_source_files_properties(qml/components/VolumeControl.qml PROPERTIES QT_RESOURCE_ALIAS VolumeControl.qml)
set_source_files_properties(qml/components/PageHeader.qml PROPERTIES QT_RESOURCE_ALIAS PageHeader.qml)
set_source_files_properties(qml/components/KeyValueRow.qml PROPERTIES QT_RESOURCE_ALIAS KeyValueRow.qml)
set_source_files_properties(qml/pages/DashboardPage.qml PROPERTIES QT_RESOURCE_ALIAS DashboardPage.qml)
set_source_files_properties(qml/pages/DevicesPage.qml PROPERTIES QT_RESOURCE_ALIAS DevicesPage.qml)
set_source_files_properties(qml/pages/SessionsPage.qml PROPERTIES QT_RESOURCE_ALIAS SessionsPage.qml)
set_source_files_properties(qml/pages/AudioRoutingPage.qml PROPERTIES QT_RESOURCE_ALIAS AudioRoutingPage.qml)
set_source_files_properties(qml/pages/DiagnosticsPage.qml PROPERTIES QT_RESOURCE_ALIAS DiagnosticsPage.qml)
set_source_files_properties(qml/pages/SettingsPage.qml PROPERTIES QT_RESOURCE_ALIAS SettingsPage.qml)
set_source_files_properties(components/StatusRow.qml PROPERTIES QT_RESOURCE_ALIAS StatusRow.qml)
set_source_files_properties(components/DeviceRow.qml PROPERTIES QT_RESOURCE_ALIAS DeviceRow.qml)
set_source_files_properties(components/EndpointRow.qml PROPERTIES QT_RESOURCE_ALIAS EndpointRow.qml)
set_source_files_properties(components/RoutePanel.qml PROPERTIES QT_RESOURCE_ALIAS RoutePanel.qml)
set_source_files_properties(components/SourceRow.qml PROPERTIES QT_RESOURCE_ALIAS SourceRow.qml)

qt_add_library(auralis-ui STATIC)

qt_add_qml_module(auralis-ui
    URI Auralis.Ui
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Auralis/Ui"
    QML_FILES
        qml/Main.qml
        qml/AppShell.qml
        qml/theme/Theme.qml
        qml/theme/Metrics.qml
        qml/navigation/NavigationRail.qml
        qml/components/StatusBadge.qml
        qml/components/SectionCard.qml
        qml/components/EmptyState.qml
        qml/components/ErrorBanner.qml
        qml/components/ConfirmDialog.qml
        qml/components/ToastHost.qml
        qml/components/VolumeControl.qml
        qml/components/PageHeader.qml
        qml/components/KeyValueRow.qml
        qml/pages/DashboardPage.qml
        qml/pages/DevicesPage.qml
        qml/pages/SessionsPage.qml
        qml/pages/AudioRoutingPage.qml
        qml/pages/DiagnosticsPage.qml
        qml/pages/SettingsPage.qml
        components/StatusRow.qml
        components/DeviceRow.qml
        components/PairingPrompt.qml
        components/EndpointRow.qml
        components/RoutePanel.qml
        components/SourceRow.qml
)

add_library(Auralis::Ui ALIAS auralis-ui)

target_link_libraries(auralis-ui
    PUBLIC
        Qt6::Quick
        Qt6::Qml
)

auralis_apply_warnings(auralis-ui)

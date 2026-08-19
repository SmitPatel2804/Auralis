import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

Item {
    id: root
    anchors.fill: parent

    readonly property var bluetooth: AppCore.bluetooth
    readonly property var audio: AppCore.audio

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: Metrics.railWidth
            Layout.fillHeight: true
            color: Theme.surfaceAlt

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Metrics.md
                spacing: Metrics.md

                Label {
                    text: qsTr("AURALIS")
                    color: Theme.text
                    font.pixelSize: 20
                    font.bold: true
                }

                NavigationRail { Layout.fillWidth: true }

                Item { Layout.fillHeight: true }

                ColumnLayout {
                    spacing: 4
                    StatusBadge {
                        label: qsTr("Bluetooth %1").arg(AppCore.bluetoothStatus)
                        kind: AppCore.bluetoothStatus === "Ready" ? "ready" : "warning"
                    }
                    StatusBadge {
                        label: qsTr("Audio %1").arg(AppCore.pipeWireStatus)
                        kind: audio && audio.connected ? "ready" : "warning"
                    }
                    StatusBadge {
                        label: AppCore.activeSessionName.length > 0 ? AppCore.activeSessionName : qsTr("No session")
                        kind: AppCore.activeSessionId.length > 0 ? "active" : "idle"
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 52
                color: Theme.surface
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Metrics.sm
                    Label {
                        text: pageTitle()
                        color: Theme.text
                        font.pixelSize: 16
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        visible: AppCore.warningCount > 0
                        text: qsTr("%1 alerts").arg(AppCore.warningCount)
                        color: Theme.warning
                    }
                }
            }

            ToastHost {
                Layout.fillWidth: true
                model: AppCore.notifications
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: AppCore.currentPage
                DashboardPage {}
                DevicesPage {}
                SessionsPage {}
                AudioRoutingPage {}
                DiagnosticsPage {}
                SettingsPage {}
            }
        }
    }

    PairingPrompt {
        bluetooth: root.bluetooth
        request: root.bluetooth ? root.bluetooth.pendingPairingRequest : null
    }

    function pageTitle() {
        switch (AppCore.currentPage) {
        case 0: return qsTr("Dashboard")
        case 1: return qsTr("Devices")
        case 2: return qsTr("Sessions")
        case 3: return qsTr("Audio Routing")
        case 4: return qsTr("Diagnostics")
        case 5: return qsTr("Settings")
        }
        return qsTr("Auralis")
    }
}

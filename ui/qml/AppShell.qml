import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

Rectangle {
    id: root
    anchors.fill: parent
    radius: 20
    color: Theme.alpha(Theme.backgroundElevated, 0.97)
    border.color: Theme.alpha(Theme.borderBright, 0.62)
    border.width: 1
    clip: true

    readonly property var bluetooth: AppCore.bluetooth
    readonly property var audio: AppCore.audio

    RowLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        Rectangle {
            Layout.preferredWidth: Metrics.railWidth
            Layout.fillHeight: true
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.railTop }
                GradientStop { position: 0.48; color: Theme.surfaceAlt }
                GradientStop { position: 1.0; color: Theme.railBottom }
            }

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.alpha(Theme.accent, 0.22)
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Metrics.md
                spacing: Metrics.lg

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.sm

                    Rectangle {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        radius: 14
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.accent }
                            GradientStop { position: 1.0; color: Theme.accentSecondary }
                        }
                        Label {
                            anchors.centerIn: parent
                            text: "A"
                            color: Theme.background
                            font.pixelSize: 21
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            text: qsTr("AURALIS")
                            color: Theme.text
                            font.pixelSize: 19
                            font.bold: true
                            font.letterSpacing: 2.2
                        }
                        Label {
                            text: qsTr("SPATIAL AUDIO CORE")
                            color: Theme.accent
                            font.pixelSize: 8
                            font.bold: true
                            font.letterSpacing: 1.15
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.alpha(Theme.borderBright, 0.5)
                }

                NavigationRail { Layout.fillWidth: true }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: statusColumn.implicitHeight + Metrics.md * 2
                    radius: Theme.radius
                    color: Theme.alpha(Theme.surfaceRaised, 0.62)
                    border.color: Theme.alpha(Theme.borderBright, 0.38)

                    ColumnLayout {
                        id: statusColumn
                        anchors.fill: parent
                        anchors.margins: Metrics.md
                        spacing: Metrics.xs

                        Label {
                            text: qsTr("SYSTEM SIGNAL")
                            color: Theme.textFaint
                            font.pixelSize: 9
                            font.bold: true
                            font.letterSpacing: 1.45
                        }
                        StatusBadge {
                            label: qsTr("Bluetooth %1").arg(AppCore.bluetoothStatus)
                            kind: AppCore.bluetoothStatus === "Ready" ? "ready" : "warning"
                        }
                        StatusBadge {
                            label: qsTr("Audio %1").arg(AppCore.audioStatus)
                            kind: audio && audio.connected ? "ready" : "warning"
                        }
                        StatusBadge {
                            label: AppCore.activeSessionName.length > 0 ? AppCore.activeSessionName : qsTr("No live session")
                            kind: AppCore.activeSessionId.length > 0 ? "active" : "idle"
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "CORE 0.1.0  //  " + AppCore.platform.operatingSystem.toUpperCase()
                    color: Theme.textFaint
                    font.pixelSize: 8
                    font.letterSpacing: 0.8
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 66
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.alpha(Theme.surfaceRaised, 0.76) }
                    GradientStop { position: 1.0; color: Theme.alpha(Theme.surface, 0.86) }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.alpha(Theme.borderBright, 0.38)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Metrics.lg
                    anchors.rightMargin: Metrics.lg
                    spacing: Metrics.md

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Label {
                            text: root.pageCode() + "  /  " + root.pageTitle().toUpperCase()
                            color: Theme.text
                            font.pixelSize: 15
                            font.bold: true
                            font.letterSpacing: 1.2
                        }
                        Label {
                            text: root.pageSubtitle()
                            color: Theme.textMuted
                            font.pixelSize: 10
                            font.letterSpacing: 0.45
                        }
                    }

                    StatusBadge {
                        label: audio && audio.connected ? qsTr("LINK STABLE") : qsTr("LINK DEGRADED")
                        kind: audio && audio.connected ? "ready" : "warning"
                    }

                    Rectangle {
                        visible: AppCore.warningCount > 0
                        implicitWidth: alertLabel.implicitWidth + Metrics.md * 2
                        implicitHeight: 32
                        radius: 10
                        color: Theme.alpha(Theme.warning, 0.12)
                        border.color: Theme.alpha(Theme.warning, 0.42)
                        Label {
                            id: alertLabel
                            anchors.centerIn: parent
                            text: qsTr("%1 ALERTS").arg(AppCore.warningCount)
                            color: Theme.warning
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 0.7
                        }
                    }
                }
            }

            ToastHost {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.lg
                Layout.rightMargin: Metrics.lg
                Layout.topMargin: Metrics.sm
                model: AppCore.notifications
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                StackLayout {
                    anchors.fill: parent
                    anchors.margins: Metrics.lg
                    anchors.topMargin: Metrics.md
                    currentIndex: AppCore.currentPage

                    // Keep pages alive after first visit so selection/controls are not destroyed.
                    Loader {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: AppCore.currentPage === 0 || status === Loader.Ready
                        sourceComponent: DashboardPage {}
                        onLoaded: if (item) item.objectName = "pageDashboard"
                    }
                    Loader {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: AppCore.currentPage === 1 || status === Loader.Ready
                        sourceComponent: DevicesPage {}
                        onLoaded: if (item) item.objectName = "pageDevices"
                    }
                    Loader {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: AppCore.currentPage === 2 || status === Loader.Ready
                        sourceComponent: SessionsPage {}
                        onLoaded: if (item) item.objectName = "pageSessions"
                    }
                    Loader {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: AppCore.currentPage === 3 || status === Loader.Ready
                        sourceComponent: PlaygroundPage {}
                        onLoaded: if (item) item.objectName = "pagePlayground"
                    }
                    Loader {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: AppCore.currentPage === 4 || status === Loader.Ready
                        sourceComponent: AudioRoutingPage {}
                        onLoaded: if (item) item.objectName = "pageAudioRouting"
                    }
                    Loader {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: AppCore.currentPage === 5 || status === Loader.Ready
                        sourceComponent: DiagnosticsPage {}
                        onLoaded: if (item) item.objectName = "pageDiagnostics"
                    }
                    Loader {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: AppCore.currentPage === 6 || status === Loader.Ready
                        sourceComponent: SettingsPage {}
                        onLoaded: if (item) item.objectName = "pageSettings"
                    }
                }
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
        case 3: return qsTr("Playground")
        case 4: return qsTr("Audio Routing")
        case 5: return qsTr("Diagnostics")
        case 6: return qsTr("Settings")
        }
        return qsTr("Auralis")
    }

    function pageCode() {
        return "0" + String(AppCore.currentPage + 1)
    }

    function pageSubtitle() {
        switch (AppCore.currentPage) {
        case 0: return qsTr("Realtime system overview")
        case 1: return qsTr("Discover, pair and inspect hardware")
        case 2: return qsTr("Compose synchronized listening groups")
        case 3: return qsTr("Pad extra delay on live session devices")
        case 4: return qsTr("Shape the native signal graph")
        case 5: return qsTr("Inspect services and telemetry")
        case 6: return qsTr("Tune the control environment")
        }
        return ""
    }
}

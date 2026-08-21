import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

Flickable {
    id: root
    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight
    boundsBehavior: Flickable.StopAtBounds

    readonly property var bluetooth: AppCore.bluetooth
    readonly property var audio: AppCore.audio
    readonly property var sessions: AppCore.sessions
    readonly property var router: audio ? audio.router : null
    readonly property bool systemReady: bluetooth && bluetooth.available && audio && audio.connected

    ColumnLayout {
        id: column
        width: root.width
        spacing: Metrics.md

        PageHeader {
            title: qsTr("Command Center")
            subtitle: qsTr("Realtime visibility across wireless hardware, signal routes and listening sessions")
        }

        ErrorBanner { text: AppCore.lastErrorText }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: heroContent.implicitHeight + Metrics.lg * 2
            radius: 20
            border.color: Theme.alpha(root.systemReady ? Theme.accent : Theme.warning, 0.45)
            border.width: 1
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.alpha(root.systemReady ? Theme.accent : Theme.warning, 0.15) }
                GradientStop { position: 0.48; color: Theme.surfaceAlt }
                GradientStop { position: 1.0; color: Theme.alpha(Theme.accentSecondary, 0.1) }
            }

            Rectangle {
                width: 210
                height: 210
                radius: 105
                anchors.right: parent.right
                anchors.rightMargin: -46
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.alpha(Theme.accent, 0.025)
                border.color: Theme.alpha(Theme.accent, 0.13)
                Rectangle {
                    anchors.centerIn: parent
                    width: 136
                    height: 136
                    radius: 68
                    color: "transparent"
                    border.color: Theme.alpha(Theme.accentSecondary, 0.16)
                }
            }

            RowLayout {
                id: heroContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Metrics.lg
                spacing: Metrics.lg

                Rectangle {
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 70
                    radius: 22
                    color: Theme.alpha(root.systemReady ? Theme.accent : Theme.warning, 0.12)
                    border.color: Theme.alpha(root.systemReady ? Theme.accent : Theme.warning, 0.48)

                    Rectangle {
                        anchors.centerIn: parent
                        width: 24
                        height: 24
                        radius: 12
                        color: root.systemReady ? Theme.success : Theme.warning
                        opacity: 0.18
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 9
                        height: 9
                        radius: 5
                        color: root.systemReady ? Theme.success : Theme.warning
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label {
                        text: root.systemReady ? qsTr("SYSTEM NOMINAL") : qsTr("ATTENTION REQUIRED")
                        color: root.systemReady ? Theme.success : Theme.warning
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.8
                    }
                    Label {
                        text: root.systemReady
                              ? qsTr("Your audio field is ready")
                              : qsTr("Bring every service online")
                        color: Theme.text
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: bluetooth && bluetooth.scanning
                              ? qsTr("Bluetooth discovery is actively mapping nearby devices.")
                              : qsTr("Native backends are synchronized and awaiting your next route.")
                        color: Theme.textMuted
                        font.pixelSize: 12
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }

                SignalButton {
                    text: bluetooth && bluetooth.scanning ? qsTr("STOP DISCOVERY") : qsTr("SCAN DEVICES")
                    primary: true
                    enabled: bluetooth && (bluetooth.canStartScan || bluetooth.canStopScan)
                    Accessible.name: text
                    onClicked: {
                        if (bluetooth.scanning)
                            bluetooth.stopScan()
                        else
                            bluetooth.startScan()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.md

            MetricTile {
                eyebrow: qsTr("Connected hardware")
                value: bluetooth ? String(bluetooth.connectedDeviceCount).padStart(2, "0") : "00"
                detail: bluetooth && bluetooth.scanning ? qsTr("DISCOVERY ACTIVE") : qsTr("BLUETOOTH LINK")
                glyph: "BT"
                signalColor: Theme.accent
            }
            MetricTile {
                eyebrow: qsTr("Audio endpoints")
                value: audio ? String(audio.endpointCount).padStart(2, "0") : "00"
                detail: AppCore.platform.audioBackend.toUpperCase()
                glyph: "IO"
                signalColor: Theme.accentSecondary
            }
            MetricTile {
                eyebrow: qsTr("Current session")
                value: sessions && sessions.currentSessionId.length > 0 ? sessions.sessionStateText : qsTr("IDLE")
                detail: AppCore.activeSessionName.length > 0 ? AppCore.activeSessionName : qsTr("NO SESSION ARMED")
                glyph: "SX"
                signalColor: sessions && sessions.currentSessionId.length > 0 ? Theme.success : Theme.textMuted
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.md

            SectionCard {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                title: qsTr("System matrix")
                subtitle: qsTr("Live readiness across native services")
                signalColor: root.systemReady ? Theme.success : Theme.warning

                KeyValueRow { label: qsTr("Bluetooth"); value: AppCore.bluetoothStatus }
                KeyValueRow { label: qsTr("Adapter"); value: bluetooth && bluetooth.adapterPowered ? qsTr("Powered") : qsTr("Unavailable") }
                KeyValueRow { label: qsTr("Discovery"); value: bluetooth && bluetooth.scanning ? qsTr("Scanning") : qsTr("Idle") }
                KeyValueRow {
                    label: AppCore.platform.audioBackend
                    value: audio && audio.connected ? qsTr("Connected") : AppCore.audioStatus
                }
                KeyValueRow {
                    label: qsTr("Known devices")
                    value: bluetooth ? String(bluetooth.deviceCount) : "0"
                }

                RowLayout {
                    Layout.fillWidth: true
                    SignalButton { text: qsTr("OPEN DEVICES"); onClicked: AppCore.navigateTo(1) }
                    SignalButton { text: qsTr("NEW SESSION"); onClicked: AppCore.navigateTo(2) }
                    Item { Layout.fillWidth: true }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                title: qsTr("Session telemetry")
                subtitle: sessions && sessions.currentSessionId.length > 0
                          ? qsTr("Live group controls")
                          : qsTr("No listening group is currently armed")
                signalColor: sessions && sessions.currentSessionId.length > 0 ? Theme.accentSecondary : Theme.textMuted

                ColumnLayout {
                    visible: sessions && sessions.currentSessionId.length > 0
                    Layout.fillWidth: true
                    spacing: Metrics.sm
                    KeyValueRow { label: qsTr("Name"); value: AppCore.activeSessionName }
                    KeyValueRow { label: qsTr("State"); value: sessions ? sessions.sessionStateText : "" }
                    KeyValueRow {
                        label: qsTr("Source")
                        value: sessions ? sessions.sourceDisplayName(sessions.currentSourceId) : ""
                    }
                    KeyValueRow {
                        label: qsTr("Members")
                        value: sessions ? qsTr("%1 expected").arg(sessions.currentMembers ? sessions.currentMembers.count : 0) : "0"
                    }
                    VolumeControl {
                        label: qsTr("Group volume")
                        value: sessions ? sessions.groupVolume : 1
                        muted: sessions ? sessions.currentMuted : false
                        onVolumeCommitted: function(v) { sessions.setGroupVolume(sessions.currentSessionId, v) }
                        onMuteToggled: function(m) { sessions.setSessionMuted(sessions.currentSessionId, m) }
                    }
                    RowLayout {
                        SignalButton {
                            text: qsTr("ACTIVATE")
                            primary: true
                            enabled: sessions && sessions.sessionStateText === "Inactive"
                            onClicked: root.report(sessions.activateSession(sessions.currentSessionId))
                        }
                        SignalButton {
                            text: qsTr("DEACTIVATE")
                            enabled: sessions && sessions.currentSessionId.length > 0 && sessions.sessionStateText !== "Inactive"
                            onClicked: root.report(sessions.deactivateSession(sessions.currentSessionId))
                        }
                        SignalButton {
                            text: qsTr("RETRY")
                            visible: sessions && (sessions.sessionStateText === "Failed" || sessions.sessionStateText === "Degraded")
                            onClicked: root.report(sessions.retrySession(sessions.currentSessionId))
                        }
                    }
                }

                EmptyState {
                    visible: !sessions || sessions.currentSessionId.length === 0
                    title: qsTr("Awaiting session")
                    message: qsTr("Create a synchronized listening group, select a source and arm its route.")
                    actionText: qsTr("OPEN SESSIONS")
                    onActionRequested: AppCore.navigateTo(2)
                }
            }
        }

        SectionCard {
            title: qsTr("Session devices")
            subtitle: qsTr("Per-device transport and endpoint state")
            visible: sessions && sessions.currentSessionId.length > 0
            signalColor: Theme.accent
            Repeater {
                model: sessions ? sessions.currentMembers : null
                delegate: RowLayout {
                    required property string displayName
                    required property bool connected
                    required property bool endpointAvailable
                    required property bool recovering
                    Layout.fillWidth: true
                    Label {
                        text: displayName
                        color: Theme.text
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    StatusBadge {
                        label: recovering ? qsTr("Recovering") : (connected ? qsTr("Connected") : qsTr("Disconnected"))
                        kind: recovering ? "recovering" : (connected ? "connected" : "warning")
                    }
                    StatusBadge {
                        label: endpointAvailable ? qsTr("Audio available") : qsTr("No endpoint")
                        kind: endpointAvailable ? "ready" : "warning"
                    }
                }
            }
        }
    }

    function report(result) {
        if (result !== 0 && AppCore.notifications)
            AppCore.notifications.postError(qsTr("Session"), sessions.commandResultText(result))
    }
}

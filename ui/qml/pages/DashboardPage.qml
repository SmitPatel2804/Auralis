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

    ColumnLayout {
        id: column
        width: root.width
        spacing: Metrics.md

        PageHeader {
            title: qsTr("Dashboard")
            subtitle: qsTr("System readiness and the current session")
        }

        ErrorBanner { text: AppCore.lastErrorText }

        SectionCard {
            title: qsTr("System readiness")
            KeyValueRow { label: qsTr("Bluetooth"); value: AppCore.bluetoothStatus }
            KeyValueRow { label: qsTr("Adapter"); value: bluetooth && bluetooth.adapterPowered ? qsTr("Powered") : qsTr("Unavailable") }
            KeyValueRow { label: qsTr("Discovery"); value: bluetooth && bluetooth.scanning ? qsTr("Scanning") : qsTr("Idle") }
            KeyValueRow { label: qsTr("PipeWire"); value: audio && audio.connected ? qsTr("Connected") : AppCore.pipeWireStatus }
            KeyValueRow {
                label: qsTr("Connected devices")
                value: bluetooth ? String(bluetooth.connectedDeviceCount) : "0"
            }
            RowLayout {
                Button {
                    text: bluetooth && bluetooth.scanning ? qsTr("Stop Scan") : qsTr("Scan for devices")
                    enabled: bluetooth && (bluetooth.canStartScan || bluetooth.canStopScan)
                    Accessible.name: text
                    onClicked: {
                        if (bluetooth.scanning)
                            bluetooth.stopScan()
                        else
                            bluetooth.startScan()
                    }
                }
                Button {
                    text: qsTr("Open Devices")
                    onClicked: AppCore.navigateTo(1)
                }
                Button {
                    text: qsTr("Create session")
                    onClicked: AppCore.navigateTo(2)
                }
            }
        }

        SectionCard {
            title: qsTr("Current session")
            visible: sessions && sessions.currentSessionId.length > 0
            KeyValueRow { label: qsTr("Name"); value: AppCore.activeSessionName }
            KeyValueRow { label: qsTr("State"); value: sessions ? sessions.sessionStateText : "" }
            KeyValueRow {
                label: qsTr("Source")
                value: sessions ? sessions.sourceDisplayName(sessions.currentSourceId) : ""
            }
            KeyValueRow {
                label: qsTr("Members")
                value: {
                    if (!sessions)
                        return "0"
                    return qsTr("%1 expected").arg(sessions.currentMembers ? sessions.currentMembers.count : 0)
                }
            }
            VolumeControl {
                label: qsTr("Group volume")
                value: sessions ? sessions.groupVolume : 1
                muted: sessions ? sessions.currentMuted : false
                onVolumeCommitted: function(v) { sessions.setGroupVolume(sessions.currentSessionId, v) }
                onMuteToggled: function(m) { sessions.setSessionMuted(sessions.currentSessionId, m) }
            }
            RowLayout {
                Button {
                    text: qsTr("Activate")
                    enabled: sessions && sessions.sessionStateText === "Inactive"
                    onClicked: root.report(sessions.activateSession(sessions.currentSessionId))
                }
                Button {
                    text: qsTr("Deactivate")
                    enabled: sessions && sessions.currentSessionId.length > 0 && sessions.sessionStateText !== "Inactive"
                    onClicked: root.report(sessions.deactivateSession(sessions.currentSessionId))
                }
                Button {
                    text: qsTr("Retry")
                    visible: sessions && (sessions.sessionStateText === "Failed" || sessions.sessionStateText === "Degraded")
                    onClicked: root.report(sessions.retrySession(sessions.currentSessionId))
                }
            }
        }

        EmptyState {
            visible: !sessions || sessions.currentSessionId.length === 0
            title: qsTr("No active session")
            message: qsTr("Create or activate a session to route audio to your devices.")
            actionText: qsTr("Open Sessions")
            onActionRequested: AppCore.navigateTo(2)
        }

        SectionCard {
            title: qsTr("Session devices")
            visible: sessions && sessions.currentSessionId.length > 0
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

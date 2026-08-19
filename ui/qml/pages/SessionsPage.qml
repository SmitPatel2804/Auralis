import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

Item {
    id: root
    readonly property var sessions: AppCore.sessions
    readonly property var bluetooth: AppCore.bluetooth
    readonly property var router: AppCore.audio ? AppCore.audio.router : null
    property string selectedId: sessions ? sessions.currentSessionId : ""

    function report(result) {
        if (result !== 0)
            AppCore.notifications.postError(qsTr("Session"), sessions.commandResultText(result))
        return result === 0
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.md

        PageHeader { title: qsTr("Sessions"); subtitle: qsTr("Create, activate, and recover multi-device sessions") }

        RowLayout {
            Button {
                text: qsTr("Create")
                Accessible.name: qsTr("Create session")
                onClicked: createDialog.open()
            }
            Button {
                text: qsTr("Rename")
                enabled: root.selectedId.length > 0
                onClicked: renameDialog.open()
            }
            Button {
                text: qsTr("Delete")
                enabled: root.selectedId.length > 0
                onClicked: deleteDialog.open()
            }
            Item { Layout.fillWidth: true }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Frame {
                SplitView.preferredWidth: 280
                ListView {
                    id: list
                    anchors.fill: parent
                    clip: true
                    model: sessions ? sessions.sessionList : null
                    delegate: ItemDelegate {
                        required property string sessionId
                        required property string name
                        required property string stateLabel
                        required property bool active
                        required property bool degraded
                        required property int deviceCount
                        required property int connectedDeviceCount
                        width: ListView.view.width
                        text: name
                        highlighted: root.selectedId === sessionId
                        onClicked: {
                            root.selectedId = sessionId
                            sessions.sessionMembers.sessionId = sessionId
                        }
                        contentItem: Column {
                            spacing: 2
                            Label { text: name; color: Theme.text; elide: Text.ElideRight; width: parent.width }
                            Row {
                                spacing: 8
                                StatusBadge { label: stateLabel; kind: degraded ? "degraded" : (active ? "active" : "idle") }
                                Label { text: qsTr("%1/%2 connected").arg(connectedDeviceCount).arg(deviceCount); color: Theme.textMuted; font.pixelSize: 11 }
                            }
                        }
                    }
                    EmptyState {
                        visible: list.count === 0
                        anchors.centerIn: parent
                        title: qsTr("No sessions")
                        message: qsTr("Create a session, add devices, choose a source, then activate.")
                    }
                }
            }

            Frame {
                SplitView.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Metrics.sm
                    spacing: Metrics.sm
                    visible: root.selectedId.length > 0

                    KeyValueRow { label: qsTr("State"); value: sessions ? sessions.sessionStateLabel(root.selectedId) : "" }
                    ComboBox {
                        Layout.fillWidth: true
                        model: router ? router.sources : null
                        textRole: "name"
                        valueRole: "sourceId"
                        displayText: currentIndex >= 0 ? currentText : qsTr("Select source")
                        onActivated: report(sessions.setSource(root.selectedId, currentValue))
                    }
                    RowLayout {
                        Button { text: qsTr("Activate"); onClicked: report(sessions.activateSession(root.selectedId)) }
                        Button { text: qsTr("Deactivate"); onClicked: report(sessions.deactivateSession(root.selectedId)) }
                        Button { text: qsTr("Retry"); onClicked: report(sessions.retrySession(root.selectedId)) }
                    }
                    VolumeControl {
                        label: qsTr("Group volume")
                        value: {
                            var v = 1
                            // bind to sessionList updates
                            return sessions ? sessions.groupVolume : 1
                        }
                        onVolumeCommitted: function(v) { report(sessions.setGroupVolume(root.selectedId, v)) }
                        onMuteToggled: function(m) { report(sessions.setSessionMuted(root.selectedId, m)) }
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        model: [ "ReconnectAndRestore", "RestoreRoutesOnly", "None" ]
                        onActivated: report(sessions.setRecoveryPolicy(root.selectedId, currentText))
                    }

                    Label { text: qsTr("Members"); color: Theme.text; font.bold: true }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: sessions ? sessions.sessionMembers : null
                        delegate: RowLayout {
                            required property string deviceId
                            required property string displayName
                            required property bool connected
                            required property bool endpointAvailable
                            required property real volume
                            required property bool muted
                            width: ListView.view.width
                            Label { text: displayName; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                            StatusBadge { label: connected ? qsTr("Connected") : qsTr("Disconnected"); kind: connected ? "connected" : "warning" }
                            Slider {
                                from: 0; to: 1; value: volume
                                onPressedChanged: if (!pressed) report(sessions.setDeviceVolume(root.selectedId, deviceId, value))
                            }
                            Button { text: qsTr("Remove"); onClicked: report(sessions.removeDevice(root.selectedId, deviceId)) }
                        }
                    }
                    ComboBox {
                        id: addDeviceCombo
                        Layout.fillWidth: true
                        model: bluetooth ? bluetooth.devices : null
                        textRole: "displayName"
                        valueRole: "objectPath"
                        displayText: qsTr("Add device")
                        onActivated: {
                            report(sessions.addDevice(root.selectedId, currentValue))
                            currentIndex = -1
                        }
                    }
                }
                EmptyState {
                    visible: root.selectedId.length === 0
                    anchors.centerIn: parent
                    title: qsTr("Select a session")
                }
            }
        }
    }

    Dialog {
        id: createDialog
        title: qsTr("Create session")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        TextField { id: createName; placeholderText: qsTr("Session name") }
        onAccepted: {
            const id = sessions.createSession(createName.text)
            if (id && id.length > 0) {
                root.selectedId = id
                sessions.sessionMembers.sessionId = id
            } else {
                AppCore.notifications.postError(qsTr("Session"), qsTr("Could not create session"))
            }
            createName.text = ""
        }
    }

    Dialog {
        id: renameDialog
        title: qsTr("Rename session")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        TextField { id: renameName; placeholderText: qsTr("New name") }
        onAccepted: {
            report(sessions.renameSession(root.selectedId, renameName.text))
            renameName.text = ""
        }
    }

    ConfirmDialog {
        id: deleteDialog
        title: qsTr("Delete session")
        message: qsTr("Delete the selected session?")
        confirmText: qsTr("Delete")
        onConfirmed: {
            if (report(sessions.deleteSession(root.selectedId)))
                root.selectedId = ""
        }
    }
}

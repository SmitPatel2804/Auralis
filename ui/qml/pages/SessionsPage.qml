import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

Item {
    id: root
    readonly property var sessions: AppCore.sessions
    readonly property var bluetooth: AppCore.bluetooth
    readonly property var router: AppCore.audio ? AppCore.audio.router : null
    readonly property var selected: sessions ? sessions.selectedSession : null
    property string selectedId: ""
    readonly property int memberCount: sessions && sessions.sessionMembers ? sessions.sessionMembers.count : 0
    readonly property int sourceCount: router ? router.sourceCount : 0
    readonly property bool canActivate: selected && selected.exists
        && selected.sourceId.length > 0 && root.memberCount > 0
    readonly property bool canRetry: selected && selected.exists
        && (selected.stateLabel === "Failed" || selected.stateLabel === "Degraded")
    readonly property bool canDeactivate: selected && selected.exists && selected.stateLabel !== "Inactive"

    onSelectedIdChanged: {
        if (selected)
            selected.sessionId = root.selectedId
        if (sessions && sessions.sessionMembers)
            sessions.sessionMembers.sessionId = root.selectedId
    }

    function report(result) {
        if (result !== 0)
            AppCore.notifications.postError(qsTr("Session"), sessions.commandResultText(result))
        return result === 0
    }

    function ensureSelection() {
        if (root.selectedId.length > 0 || !sessions || !sessions.sessionList)
            return
        if (sessions.sessionList.rowCount() <= 0)
            return
        const first = sessions.sessionList.sessionIdAt(0)
        if (first && String(first).length > 0)
            root.selectedId = String(first)
    }

    function syncSourceCombo() {
        if (!sourceCombo || !selected)
            return
        const idx = sourceCombo.indexOfValue(selected.sourceId)
        if (sourceCombo.currentIndex !== idx)
            sourceCombo.currentIndex = idx
    }

    Connections {
        target: router
        function onSourcesChanged() { Qt.callLater(root.syncSourceCombo) }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.md

        PageHeader { title: qsTr("Session Orchestrator"); subtitle: qsTr("Compose, activate, and recover synchronized listening groups") }

        Label {
            visible: Qt.platform.os === "windows"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.warning
            text: qsTr("Windows safety: do not use the same headset as both the Windows default output and an Auralis session destination. Auralis now rejects that delayed duplicate path.")
        }

        RowLayout {
            SignalButton {
                text: qsTr("Create")
                primary: true
                Accessible.name: qsTr("Create session")
                onClicked: createDialog.open()
            }
            SignalButton {
                text: qsTr("Rename")
                enabled: root.selectedId.length > 0
                onClicked: renameDialog.open()
            }
            SignalButton {
                text: qsTr("Duplicate")
                enabled: root.selectedId.length > 0
                Accessible.name: qsTr("Duplicate session")
                onClicked: {
                    const id = sessions.duplicateSession(root.selectedId)
                    if (id && id.length > 0) {
                        root.selectedId = id
                    } else {
                        AppCore.notifications.postError(qsTr("Session"), qsTr("Could not duplicate session"))
                    }
                }
            }
            SignalButton {
                text: qsTr("Delete")
                danger: true
                enabled: root.selectedId.length > 0
                onClicked: deleteDialog.open()
            }
            SignalButton {
                text: qsTr("Restore last session")
                Accessible.name: qsTr("Restore last session")
                onClicked: {
                    if (root.report(sessions.restoreLastSession()))
                        root.selectedId = sessions.currentSessionId
                }
            }
            Item { Layout.fillWidth: true }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            handle: Rectangle {
                implicitWidth: 5
                color: Theme.alpha(Theme.accent, SplitHandle.hovered || SplitHandle.pressed ? 0.55 : 0.15)
                Rectangle {
                    anchors.centerIn: parent
                    width: 1
                    height: 54
                    color: Theme.alpha(Theme.accent, 0.65)
                }
            }

            Frame {
                SplitView.preferredWidth: 280
                SplitView.minimumWidth: 200
                padding: Metrics.xs
                background: Rectangle {
                    radius: Theme.cardRadius
                    color: Theme.alpha(Theme.surface, 0.9)
                    border.color: Theme.alpha(Theme.accentSecondary, 0.24)
                }
                ListView {
                    id: list
                    anchors.fill: parent
                    clip: true
                    model: sessions ? sessions.sessionList : null
                    onCountChanged: root.ensureSelection()
                    Component.onCompleted: root.ensureSelection()
                    delegate: ItemDelegate {
                        required property int index
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
                        hoverEnabled: true
                        background: Rectangle {
                            radius: 10
                            color: highlighted
                                   ? Theme.alpha(Theme.accentSecondary, 0.18)
                                   : (parent.hovered ? Theme.surfaceHover : "transparent")
                            border.color: highlighted
                                          ? Theme.alpha(Theme.accentSecondary, 0.55)
                                          : "transparent"
                        }
                        onClicked: root.selectedId = sessionId
                        Component.onCompleted: {
                            if (index === 0)
                                root.ensureSelection()
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
                SplitView.minimumWidth: 320
                padding: 0
                background: Rectangle {
                    radius: Theme.cardRadius
                    color: Theme.alpha(Theme.surface, 0.82)
                    border.color: Theme.alpha(Theme.accent, 0.2)
                }

                EmptyState {
                    visible: !selected || !selected.exists
                    anchors.centerIn: parent
                    title: qsTr("Select a session")
                }

                ScrollView {
                    id: detailScroll
                    anchors.fill: parent
                    anchors.margins: Metrics.sm
                    visible: selected && selected.exists
                    clip: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: detailScroll.availableWidth
                        spacing: Metrics.sm

                        KeyValueRow { label: qsTr("State"); value: selected ? selected.stateLabel : "" }

                        Label {
                            text: qsTr("Audio source")
                            color: Theme.text
                            font.bold: true
                        }
                        Label {
                            text: root.sourceCount === 0
                                  ? qsTr("No application audio sessions yet. Start playback in a browser or app; it will appear automatically.")
                                  : qsTr("Choose the browser or application whose playback should be shared with this session.")
                            color: Theme.textMuted
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            font.pixelSize: 12
                        }
                        SignalComboBox {
                            id: sourceCombo
                            objectName: "sessionSourceSelector"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            Layout.minimumHeight: 36
                            model: router ? router.sources : null
                            textRole: "name"
                            valueRole: "sourceId"
                            enabled: root.sourceCount > 0
                            displayText: {
                                if (currentIndex >= 0)
                                    return currentText
                                if (selected && selected.sourceName && selected.sourceName.length > 0)
                                    return selected.sourceName
                                return qsTr("Select source")
                            }
                            Component.onCompleted: root.syncSourceCombo()
                            onModelChanged: Qt.callLater(root.syncSourceCombo)
                            onActivated: root.report(sessions.setSource(root.selectedId, currentValue))
                            popup.implicitHeight: Math.min(360, 40 * Math.max(1, root.sourceCount))
                        }

                        RowLayout {
                            SignalButton {
                                text: qsTr("Activate")
                                primary: true
                                enabled: root.canActivate
                                onClicked: root.report(sessions.activateSession(root.selectedId))
                            }
                            SignalButton {
                                text: qsTr("Deactivate")
                                enabled: root.canDeactivate
                                onClicked: root.report(sessions.deactivateSession(root.selectedId))
                            }
                            SignalButton {
                                text: qsTr("Retry")
                                enabled: root.canRetry
                                onClicked: root.report(sessions.retrySession(root.selectedId))
                            }
                        }

                        VolumeControl {
                            Layout.fillWidth: true
                            label: qsTr("Group volume")
                            value: selected ? selected.groupVolume : 1
                            muted: selected ? selected.muted : false
                            onVolumeCommitted: function(v) { root.report(sessions.setGroupVolume(root.selectedId, v)) }
                            onMuteToggled: function(m) { root.report(sessions.setSessionMuted(root.selectedId, m)) }
                        }

                        Label {
                            text: qsTr("Recovery policy")
                            color: Theme.text
                            font.bold: true
                        }
                        SignalComboBox {
                            id: policyCombo
                            objectName: "sessionPolicySelector"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            model: [ "ReconnectAndRestore", "RestoreRoutesOnly", "None" ]
                            currentIndex: selected ? model.indexOf(selected.recoveryPolicy) : 0
                            onActivated: root.report(sessions.setRecoveryPolicy(root.selectedId, currentText))
                        }

                        Label { text: qsTr("Members"); color: Theme.text; font.bold: true }
                        Frame {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.max(120, Math.min(220, 48 * Math.max(1, root.memberCount)))
                            padding: Metrics.sm
                            background: Rectangle {
                                radius: 12
                                color: Theme.alpha(Theme.backgroundElevated, 0.68)
                                border.color: Theme.border
                            }
                            ListView {
                                anchors.fill: parent
                                clip: true
                                model: sessions ? sessions.sessionMembers : null
                                delegate: RowLayout {
                                    required property string deviceId
                                    required property string displayName
                                    required property bool connected
                                    required property bool endpointAvailable
                                    required property real volume
                                    required property bool muted
                                    required property bool memberEnabled
                                    width: ListView.view.width
                                    Label { text: displayName; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                                    StatusBadge {
                                        label: !memberEnabled ? qsTr("Released") : (connected ? qsTr("Connected") : qsTr("Disconnected"))
                                        kind: !memberEnabled ? "idle" : (connected ? "connected" : "warning")
                                    }
                                    Slider {
                                        from: 0; to: 1; value: volume
                                        onPressedChanged: if (!pressed) root.report(sessions.setDeviceVolume(root.selectedId, deviceId, value))
                                    }
                                    SignalButton {
                                        text: memberEnabled ? qsTr("Release") : qsTr("Enable")
                                        compact: true
                                        primary: !memberEnabled
                                        onClicked: root.report(sessions.setDeviceEnabled(root.selectedId, deviceId, !memberEnabled))
                                    }
                                    SignalButton {
                                        text: qsTr("Remove")
                                        compact: true
                                        danger: true
                                        onClicked: root.report(sessions.removeDevice(root.selectedId, deviceId))
                                    }
                                }
                                EmptyState {
                                    visible: root.memberCount === 0
                                    anchors.centerIn: parent
                                    title: qsTr("No devices")
                                    message: qsTr("Add Smokin' Buds below.")
                                }
                            }
                        }
                        SignalComboBox {
                            id: addDeviceCombo
                            objectName: "sessionDeviceSelector"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            model: bluetooth ? bluetooth.classicDevices : null
                            textRole: "displayName"
                            valueRole: "address"
                            displayText: qsTr("Add device")
                            onActivated: {
                                const address = currentValue
                                if (address === undefined || address === null || String(address).length === 0) {
                                    AppCore.notifications.postError(qsTr("Session"), qsTr("Select a device with a Bluetooth address"))
                                } else {
                                    root.report(sessions.addDevice(root.selectedId, String(address)))
                                }
                                currentIndex = -1
                            }
                        }
                    }
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
        contentItem: ColumnLayout {
            spacing: Metrics.xs
            Label { text: qsTr("Session name"); color: Theme.text }
            TextField {
                id: createName
                Layout.fillWidth: true
                placeholderText: qsTr("Enter a session name")
                Accessible.name: qsTr("Session name")
            }
        }
        onOpened: createName.forceActiveFocus()
        onAccepted: {
            const id = sessions.createSession(createName.text)
            if (id && id.length > 0) {
                root.selectedId = id
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
        contentItem: ColumnLayout {
            spacing: Metrics.xs
            Label { text: qsTr("New session name"); color: Theme.text }
            TextField {
                id: renameName
                Layout.fillWidth: true
                placeholderText: qsTr("Enter a new name")
                Accessible.name: qsTr("New session name")
            }
        }
        onOpened: renameName.forceActiveFocus()
        onAccepted: {
            root.report(sessions.renameSession(root.selectedId, renameName.text))
            renameName.text = ""
        }
    }

    ConfirmDialog {
        id: deleteDialog
        title: qsTr("Delete session")
        message: qsTr("Delete the selected session?")
        confirmText: qsTr("Delete")
        onConfirmed: {
            if (root.report(sessions.deleteSession(root.selectedId)))
                root.selectedId = ""
        }
    }
}

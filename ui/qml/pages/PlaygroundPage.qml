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

    readonly property var sessions: AppCore.sessions
    readonly property string liveSessionId: AppCore.activeSessionId
    readonly property bool hasLiveSession: root.liveSessionId.length > 0
    readonly property int memberCount: sessions && sessions.currentMembers ? sessions.currentMembers.count : 0

    function report(result) {
        if (result !== 0 && sessions)
            AppCore.notifications.postError(qsTr("Playground"), sessions.commandResultText(result))
        return result === 0
    }

    function nudge(deviceId, deltaMs) {
        if (!sessions || !root.hasLiveSession)
            return
        root.report(sessions.nudgeDeviceDelayMs(root.liveSessionId, deviceId, deltaMs))
    }

    function resetDelay(deviceId) {
        if (!sessions || !root.hasLiveSession)
            return
        root.report(sessions.setDeviceDelayMs(root.liveSessionId, deviceId, 0))
    }

    ColumnLayout {
        id: column
        width: root.width
        spacing: Metrics.md

        PageHeader {
            title: qsTr("Playground")
            subtitle: qsTr("Add extra delay to individual devices in the live session")
        }

        EmptyState {
            visible: !root.hasLiveSession
            title: qsTr("No live session")
            message: qsTr("Activate a session first, then come back here to pad each headset independently.")
            actionText: qsTr("OPEN SESSIONS")
            onActionRequested: AppCore.navigateTo(2)
        }

        EmptyState {
            visible: root.hasLiveSession && root.memberCount === 0
            title: qsTr("No devices")
            message: qsTr("The live session has no members. Add headsets on Sessions, then activate again.")
            actionText: qsTr("OPEN SESSIONS")
            onActionRequested: AppCore.navigateTo(2)
        }

        Repeater {
            model: root.hasLiveSession && sessions ? sessions.currentMembers : null
            delegate: SectionCard {
                required property string deviceId
                required property string displayName
                required property real delayMs
                required property bool connected
                required property bool routeActive

                title: displayName
                subtitle: routeActive ? qsTr("Live extra delay") : qsTr("Saved extra delay — applies when this device is routed")
                signalColor: routeActive ? Theme.accent : Theme.warning

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("%1 ms").arg(Math.round(delayMs))
                        color: Theme.text
                        font.pixelSize: 22
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    StatusBadge {
                        label: connected ? qsTr("Connected") : qsTr("Disconnected")
                        kind: connected ? "connected" : "warning"
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Metrics.xs
                    SignalButton {
                        compact: true
                        text: qsTr("-10ms")
                        enabled: delayMs > 0
                        onClicked: root.nudge(deviceId, -10)
                    }
                    SignalButton {
                        compact: true
                        text: qsTr("-5ms")
                        enabled: delayMs > 0
                        onClicked: root.nudge(deviceId, -5)
                    }
                    SignalButton {
                        compact: true
                        text: qsTr("-1ms")
                        enabled: delayMs > 0
                        onClicked: root.nudge(deviceId, -1)
                    }
                    SignalButton {
                        compact: true
                        text: qsTr("Reset")
                        enabled: delayMs > 0
                        onClicked: root.resetDelay(deviceId)
                    }
                    SignalButton {
                        compact: true
                        text: qsTr("+1ms")
                        onClicked: root.nudge(deviceId, 1)
                    }
                    SignalButton {
                        compact: true
                        text: qsTr("+5ms")
                        onClicked: root.nudge(deviceId, 5)
                    }
                    SignalButton {
                        compact: true
                        text: qsTr("+10ms")
                        onClicked: root.nudge(deviceId, 10)
                    }
                }
            }
        }

        Label {
            visible: root.hasLiveSession && root.memberCount > 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textMuted
            font.pixelSize: 12
            text: qsTr("Delay is extra padding on that device only, from 0 to 250 ms. Dual Bluetooth still cannot be sample-perfect.")
        }
    }
}

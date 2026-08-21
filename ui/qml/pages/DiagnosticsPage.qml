import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

Item {
    id: root
    readonly property var bluetooth: AppCore.bluetooth
    readonly property var audio: AppCore.audio
    readonly property var sessions: AppCore.sessions
    readonly property var logs: AppCore.diagnostics
    readonly property var router: audio ? audio.router : null

    Component.onCompleted: {
        if (logs)
            logs.captureEnabled = true
    }
    Component.onDestruction: {
        if (logs)
            logs.captureEnabled = false
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.md

        PageHeader { title: qsTr("Telemetry Console"); subtitle: qsTr("Live subsystem state and event intelligence") }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 420
            clip: true
            ColumnLayout {
                width: parent.width
                spacing: Metrics.md
                SectionCard {
                    title: qsTr("System")
                    subtitle: qsTr("Cross-platform services and runtime health")
                    signalColor: Theme.accent
                    KeyValueRow { label: qsTr("Core"); value: AppCore.coreStatus }
                    KeyValueRow { label: qsTr("Operating system"); value: AppCore.platform.operatingSystem }
                    KeyValueRow { label: qsTr("Bluetooth backend"); value: AppCore.platform.bluetoothBackend }
                    KeyValueRow { label: qsTr("Audio backend"); value: AppCore.platform.audioBackend }
                    KeyValueRow { label: qsTr("Power backend"); value: AppCore.platform.powerBackend }
                    KeyValueRow { label: qsTr("Recovery"); value: AppCore.recoveryStatus }
                    KeyValueRow { label: qsTr("Bluetooth"); value: AppCore.bluetoothStatus }
                    KeyValueRow { label: AppCore.platform.audioBackend; value: audio ? audio.connectionStateText : AppCore.audioStatus }
                    KeyValueRow { label: qsTr("Session"); value: sessions ? sessions.sessionStateText : "" }
                    KeyValueRow { label: qsTr("Devices"); value: bluetooth ? String(bluetooth.deviceCount) : "0" }
                    KeyValueRow { label: qsTr("Endpoints"); value: audio ? String(audio.endpointCount) : "0" }
                }
                SectionCard {
                    title: qsTr("Bluetooth")
                    subtitle: qsTr("Radio adapter and discovery state")
                    signalColor: Theme.accentSecondary
                    KeyValueRow { label: qsTr("Adapter"); value: bluetooth ? bluetooth.adapterName : "" }
                    KeyValueRow { label: qsTr("Powered"); value: bluetooth && bluetooth.adapterPowered ? qsTr("Yes") : qsTr("No") }
                    KeyValueRow { label: qsTr("Discovering"); value: bluetooth && bluetooth.scanning ? qsTr("Yes") : qsTr("No") }
                    KeyValueRow { label: qsTr("Error"); value: bluetooth ? bluetooth.errorText : "" }
                }
                SectionCard {
                    title: AppCore.platform.audioBackend
                    subtitle: qsTr("Native audio graph and mapped endpoints")
                    signalColor: Theme.info
                    KeyValueRow { label: qsTr("Nodes"); value: audio ? String(audio.nodeCount) : "0" }
                    KeyValueRow { label: qsTr("Mapped BT"); value: audio ? String(audio.mappedBluetoothCount) : "0" }
                    Label {
                        visible: AppCore.showDeveloperStatus && audio
                        text: audio ? audio.diagnosticsText : ""
                        color: Theme.textMuted
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
                SectionCard {
                    title: qsTr("Devices")
                    signalColor: Theme.success
                    Repeater {
                        model: bluetooth ? bluetooth.devices : null
                        delegate: KeyValueRow {
                            required property string displayName
                            required property bool connected
                            required property bool paired
                            required property string address
                            label: displayName
                            value: (connected ? qsTr("Connected") : qsTr("Disconnected"))
                                   + " · " + (paired ? qsTr("Paired") : qsTr("Unpaired"))
                                   + (address.length > 0 ? (" · " + address) : "")
                        }
                    }
                    Label {
                        visible: !bluetooth || bluetooth.deviceCount === 0
                        text: qsTr("No Bluetooth devices")
                        color: Theme.textMuted
                    }
                }
                SectionCard {
                    title: qsTr("Routes")
                    signalColor: Theme.accentSecondary
                    Repeater {
                        model: router ? router.routes : null
                        delegate: KeyValueRow {
                            required property string sourceName
                            required property string ownerLabel
                            required property string stateText
                            required property int destinationCount
                            label: sourceName
                            value: ownerLabel + " · " + stateText + " · "
                                   + qsTr("%1 destinations").arg(destinationCount)
                        }
                    }
                    Label {
                        visible: !router || (router.routes && router.routes.rowCount() === 0)
                        text: qsTr("No routes")
                        color: Theme.textMuted
                    }
                }
                SectionCard {
                    title: qsTr("Current session members")
                    signalColor: Theme.warning
                    Repeater {
                        model: sessions ? sessions.currentMembers : null
                        delegate: KeyValueRow {
                            required property string displayName
                            required property bool connected
                            required property bool recovering
                            required property bool routeActive
                            required property bool endpointAvailable
                            label: displayName
                            value: (recovering ? qsTr("Recovering") : (connected ? qsTr("Connected") : qsTr("Disconnected")))
                                   + " · " + (routeActive ? qsTr("Route active") : qsTr("No route"))
                                   + " · " + (endpointAvailable ? qsTr("Endpoint") : qsTr("No endpoint"))
                        }
                    }
                    Label {
                        visible: !sessions || sessions.currentSessionId.length === 0
                        text: qsTr("No active session")
                        color: Theme.textMuted
                    }
                }
            }
        }

        RowLayout {
            ComboBox {
                model: ["", "DEBUG", "INFO", "WARNING", "CRITICAL"]
                displayText: currentIndex <= 0 ? qsTr("All severities") : currentText
                onActivated: logs.severityFilter = currentIndex <= 0 ? "" : currentText
            }
            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Filter category")
                onTextChanged: logs.categoryFilter = text
            }
            SignalButton {
                text: qsTr("Copy visible")
                primary: true
                onClicked: {
                    if (logs.copyVisibleToClipboard())
                        AppCore.notifications.postInfo(qsTr("Diagnostics"), qsTr("Visible log copied to clipboard"))
                    else
                        AppCore.notifications.postError(qsTr("Diagnostics"), qsTr("Clipboard is unavailable"))
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: logs
            spacing: 2
            Rectangle {
                anchors.fill: parent
                z: -1
                radius: Theme.cardRadius
                color: Theme.alpha(Theme.backgroundElevated, 0.82)
                border.color: Theme.border
            }
            delegate: Label {
                required property string timestamp
                required property string severity
                required property string category
                required property string message
                width: ListView.view.width
                text: timestamp + " " + severity + " " + category + " " + message
                color: Theme.textMuted
                wrapMode: Text.WrapAnywhere
                font.pixelSize: 11
                leftPadding: Metrics.sm
                rightPadding: Metrics.sm
                topPadding: 4
                bottomPadding: 4
                background: Rectangle {
                    radius: 6
                    color: index % 2 === 0 ? Theme.alpha(Theme.surfaceRaised, 0.28) : "transparent"
                }
            }
        }
    }
}

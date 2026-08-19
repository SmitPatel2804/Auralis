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

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.md

        PageHeader { title: qsTr("Diagnostics"); subtitle: qsTr("Live backend state without a terminal") }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 280
            clip: true
            ColumnLayout {
                width: parent.width
                spacing: Metrics.md
                SectionCard {
                    title: qsTr("System")
                    KeyValueRow { label: qsTr("Core"); value: AppCore.coreStatus }
                    KeyValueRow { label: qsTr("Bluetooth"); value: AppCore.bluetoothStatus }
                    KeyValueRow { label: qsTr("PipeWire"); value: audio ? audio.connectionStateText : AppCore.pipeWireStatus }
                    KeyValueRow { label: qsTr("Session"); value: sessions ? sessions.sessionStateText : "" }
                    KeyValueRow { label: qsTr("Devices"); value: bluetooth ? String(bluetooth.deviceCount) : "0" }
                    KeyValueRow { label: qsTr("Endpoints"); value: audio ? String(audio.endpointCount) : "0" }
                }
                SectionCard {
                    title: qsTr("Bluetooth")
                    KeyValueRow { label: qsTr("Adapter"); value: bluetooth ? bluetooth.adapterName : "" }
                    KeyValueRow { label: qsTr("Powered"); value: bluetooth && bluetooth.adapterPowered ? qsTr("Yes") : qsTr("No") }
                    KeyValueRow { label: qsTr("Discovering"); value: bluetooth && bluetooth.scanning ? qsTr("Yes") : qsTr("No") }
                    KeyValueRow { label: qsTr("Error"); value: bluetooth ? bluetooth.errorText : "" }
                }
                SectionCard {
                    title: qsTr("PipeWire")
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
            Button {
                text: qsTr("Copy visible")
                onClicked: {
                    const text = logs.visibleText()
                    if (text.length > 0)
                        AppCore.notifications.postInfo(qsTr("Diagnostics"), qsTr("Log copied to clipboard is not wired; text length %1").arg(text.length))
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: logs
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
            }
        }
    }
}

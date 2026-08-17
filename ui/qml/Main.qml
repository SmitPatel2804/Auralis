import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 720
    height: 480
    title: "Auralis"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 16

        Text {
            text: "AURALIS"
            font.pixelSize: 28
            font.bold: true
        }

        Text {
            text: "System Status"
            font.pixelSize: 18
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: "#cccccc"
        }

        StatusRow {
            label: "Bluetooth"
            status: AppCore.bluetoothStatus
        }

        StatusRow {
            label: "Audio"
            status: AppCore.audioStatus
        }

        StatusRow {
            label: "PipeWire"
            status: AppCore.pipeWireStatus
        }

        Text {
            text: AppCore.ready ? "Auralis Core Ready" : "Auralis Core Not Ready"
            font.pixelSize: 16
        }

        Text {
            visible: AppCore.showDeveloperStatus
            text: "Foundation services initialized — hardware integration begins in Phase 2+"
            font.pixelSize: 12
            color: "#666666"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Item {
            Layout.fillHeight: true
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 760
    height: 720
    title: "Auralis"

    readonly property QtObject bluetooth: AppCore.bluetooth

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
            text: "Bluetooth Ready means the discovery subsystem is initialized. It does not mean an adapter or nearby device was found."
            font.pixelSize: 12
            color: "#666666"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: "Bluetooth Discovery"
            font.pixelSize: 18
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: "#cccccc"
        }

        StatusRow {
            label: "BlueZ"
            status: root.bluetooth && root.bluetooth.available ? "Ready" : "Unavailable"
        }

        StatusRow {
            label: "Adapter"
            status: root.bluetooth && root.bluetooth.adapterName.length > 0
                    ? (root.bluetooth.adapterName + " / " + root.bluetooth.adapterAddress)
                    : "Not available"
        }

        StatusRow {
            label: "Power"
            status: root.bluetooth && root.bluetooth.adapterPowered ? "On" : "Off"
        }

        StatusRow {
            label: "Scan"
            status: root.bluetooth && root.bluetooth.scanning ? "Active" : "Idle"
        }

        StatusRow {
            label: "Devices"
            status: root.bluetooth ? String(root.bluetooth.deviceCount) : "0"
        }

        Text {
            visible: root.bluetooth && root.bluetooth.statusText.length > 0
            text: root.bluetooth ? root.bluetooth.statusText : ""
            font.pixelSize: 14
            color: root.bluetooth && root.bluetooth.scanning ? "#1565c0" : "#333333"
        }

        Text {
            visible: root.bluetooth && root.bluetooth.errorText.length > 0
            text: root.bluetooth ? root.bluetooth.errorText : ""
            font.pixelSize: 13
            color: "#c62828"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: 12

            Button {
                text: "Start Scan"
                enabled: root.bluetooth && root.bluetooth.canStartScan
                onClicked: root.bluetooth.startScan()
            }

            Button {
                text: "Stop Scan"
                enabled: root.bluetooth && root.bluetooth.canStopScan
                onClicked: root.bluetooth.stopScan()
            }

            Button {
                text: "Refresh"
                enabled: AppCore.ready && root.bluetooth
                onClicked: root.bluetooth.refresh()
            }
        }

        Text {
            text: "Nearby Devices"
            font.pixelSize: 18
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200

            ListView {
                id: deviceList
                anchors.fill: parent
                clip: true
                spacing: 12
                boundsBehavior: Flickable.StopAtBounds
                model: root.bluetooth ? root.bluetooth.devices : null

                delegate: Item {
                    id: wrapper
                    required property string displayName
                    required property string address
                    required property string addressType
                    required property string transportHint
                    required property bool hasRssi
                    required property int rssi
                    required property bool paired
                    required property bool connected
                    required property bool servicesResolved

                    width: deviceList.width
                    height: row.implicitHeight

                    DeviceRow {
                        id: row
                        width: parent.width
                        displayName: wrapper.displayName
                        address: wrapper.address
                        addressType: wrapper.addressType
                        transportHint: wrapper.transportHint
                        hasRssi: wrapper.hasRssi
                        rssi: wrapper.rssi
                        paired: wrapper.paired
                        connected: wrapper.connected
                        servicesResolved: wrapper.servicesResolved
                    }
                }
            }

            Text {
                visible: deviceList.count === 0
                anchors.centerIn: parent
                text: "No devices yet. Start a scan to discover nearby Bluetooth devices."
                color: "#666666"
            }
        }
    }
}

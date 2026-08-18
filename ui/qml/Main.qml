import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 860
    height: 980
    title: "Auralis"

    readonly property QtObject bluetooth: AppCore.bluetooth
    readonly property QtObject audio: AppCore.audio

    Dialog {
        id: servicesDialog
        title: "Device Services"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Close
        implicitWidth: 440
        implicitHeight: 360
        property var serviceEntries: []

        contentItem: ListView {
            clip: true
            implicitWidth: 400
            implicitHeight: 280
            model: servicesDialog.serviceEntries
            delegate: Item {
                required property string uuid
                required property string name
                width: ListView.view.width
                height: 40
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    Text { text: name; font.bold: true }
                    Text { text: uuid; font.pixelSize: 11; color: "#666666" }
                }
            }
        }
    }

    PairingPrompt {
        id: pairingPrompt
        bluetooth: root.bluetooth
        request: root.bluetooth ? root.bluetooth.pendingPairingRequest : null
    }

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
            healthy: AppCore.audioStatus === "Ready"
        }

        StatusRow {
            label: "PipeWire"
            status: root.audio && root.audio.connectionStateText.length > 0
                    ? root.audio.connectionStateText
                    : AppCore.pipeWireStatus
            healthy: root.audio ? root.audio.connected : false
        }

        Text {
            visible: root.audio && root.audio.lastError.length > 0
            text: root.audio ? root.audio.lastError : ""
            font.pixelSize: 13
            color: "#c62828"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
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
            label: "Agent"
            status: root.bluetooth && root.bluetooth.agentRegistered ? "Registered" : "Not registered"
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
            Layout.minimumHeight: 220

            ListView {
                id: deviceList
                anchors.fill: parent
                clip: true
                spacing: 12
                boundsBehavior: Flickable.StopAtBounds
                model: root.bluetooth ? root.bluetooth.devices : null

                delegate: Item {
                    id: wrapper
                    required property string objectPath
                    required property string displayName
                    required property string address
                    required property string addressType
                    required property string transportHint
                    required property bool hasRssi
                    required property int rssi
                    required property bool paired
                    required property bool connected
                    required property bool trusted
                    required property bool servicesResolved
                    required property string operationText
                    required property string lastErrorMessage
                    required property bool canPair
                    required property bool canCancelPairing
                    required property bool canTrust
                    required property bool canUntrust
                    required property bool canConnect
                    required property bool canDisconnect
                    required property bool canForget
                    required property bool canReconnect
                    required property bool canCancelOperation
                    required property var uuids

                    readonly property string audioStatus: root.audio
                            ? (root.audio.graphRevision, root.audio.audioStatusForDevice(wrapper.objectPath))
                            : ""

                    width: deviceList.width
                    height: row.implicitHeight

                    DeviceRow {
                        id: row
                        width: parent.width
                        objectPath: wrapper.objectPath
                        displayName: wrapper.displayName
                        address: wrapper.address
                        addressType: wrapper.addressType
                        transportHint: wrapper.transportHint
                        hasRssi: wrapper.hasRssi
                        rssi: wrapper.rssi
                        paired: wrapper.paired
                        connected: wrapper.connected
                        trusted: wrapper.trusted
                        servicesResolved: wrapper.servicesResolved
                        operationText: wrapper.operationText
                        lastErrorMessage: wrapper.lastErrorMessage
                        canPair: wrapper.canPair
                        canCancelPairing: wrapper.canCancelPairing
                        canTrust: wrapper.canTrust
                        canUntrust: wrapper.canUntrust
                        canConnect: wrapper.canConnect
                        canDisconnect: wrapper.canDisconnect
                        canForget: wrapper.canForget
                        canReconnect: wrapper.canReconnect
                        canCancelOperation: wrapper.canCancelOperation
                        uuids: wrapper.uuids
                        audioStatus: wrapper.audioStatus

                        onPairRequested: root.bluetooth.pairDevice(wrapper.objectPath)
                        onCancelPairingRequested: root.bluetooth.cancelPairing(wrapper.objectPath)
                        onCancelOperationRequested: root.bluetooth.cancelDeviceOperation(wrapper.objectPath)
                        onTrustRequested: root.bluetooth.trustDevice(wrapper.objectPath)
                        onUntrustRequested: root.bluetooth.untrustDevice(wrapper.objectPath)
                        onConnectRequested: root.bluetooth.connectDevice(wrapper.objectPath)
                        onDisconnectRequested: root.bluetooth.disconnectDevice(wrapper.objectPath)
                        onForgetRequested: root.bluetooth.forgetDevice(wrapper.objectPath)
                        onReconnectRequested: root.bluetooth.reconnectDevice(wrapper.objectPath)
                        onShowServicesRequested: {
                            var entries = []
                            for (var i = 0; i < wrapper.uuids.length; ++i) {
                                var uuid = wrapper.uuids[i]
                                entries.push({
                                    uuid: uuid,
                                    name: root.bluetooth.serviceFriendlyName(uuid)
                                })
                            }
                            servicesDialog.serviceEntries = entries
                            servicesDialog.open()
                        }
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

        Text {
            text: "Audio Endpoints"
            font.pixelSize: 18
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: "#cccccc"
        }

        Text {
            visible: AppCore.showDeveloperStatus && root.audio
            text: root.audio ? root.audio.diagnosticsText : ""
            font.pixelSize: 11
            color: "#666666"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160

            ListView {
                id: endpointList
                anchors.fill: parent
                clip: true
                spacing: 10
                boundsBehavior: Flickable.StopAtBounds
                model: root.audio ? root.audio.endpoints : null

                delegate: Item {
                    id: endpointWrapper
                    required property string name
                    required property string direction
                    required property bool available
                    required property string transport
                    required property string profile
                    required property string codec
                    required property bool mapped
                    required property string bluetoothDisplayName
                    required property string bluetoothAddress
                    required property int pipeWireObjectId

                    width: endpointList.width
                    height: endpointRow.implicitHeight

                    EndpointRow {
                        id: endpointRow
                        width: parent.width
                        name: endpointWrapper.name
                        direction: endpointWrapper.direction
                        available: endpointWrapper.available
                        transport: endpointWrapper.transport
                        profile: endpointWrapper.profile
                        codec: endpointWrapper.codec
                        mapped: endpointWrapper.mapped
                        bluetoothDisplayName: endpointWrapper.bluetoothDisplayName
                        bluetoothAddress: endpointWrapper.bluetoothAddress
                        pipeWireObjectId: endpointWrapper.pipeWireObjectId
                        showDeveloperDetail: AppCore.showDeveloperStatus
                    }
                }
            }

            Text {
                visible: endpointList.count === 0
                anchors.centerIn: parent
                text: root.audio && root.audio.connected
                      ? "No audio endpoints classified yet."
                      : "PipeWire endpoints appear here once the audio graph is connected."
                color: "#666666"
            }
        }
    }
}

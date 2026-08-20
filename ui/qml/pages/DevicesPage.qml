import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

Item {
    id: root
    readonly property var bluetooth: AppCore.bluetooth
    readonly property var audio: AppCore.audio

    property string filter: "all"
    property string query: ""
    property string selectedPath: ""
    property int audioGraphRevision: audio ? audio.graphRevision : 0
    readonly property bool selectedAvailable: bluetooth && selectedPath.length > 0 && bluetooth.hasDevice(selectedPath)
    readonly property var details: {
        if (!bluetooth || !selectedAvailable)
            return ({})
        bluetooth.deviceCount
        bluetooth.connectedDeviceCount
        return bluetooth.deviceDetails(selectedPath)
    }

    function clearStaleSelection() {
        if (root.selectedPath.length > 0 && bluetooth && !bluetooth.hasDevice(root.selectedPath))
            root.selectedPath = ""
    }

    Connections {
        target: bluetooth && bluetooth.devices ? bluetooth.devices : null
        function onRowsRemoved() { root.clearStaleSelection() }
        function onModelReset() { root.clearStaleSelection() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.md

        PageHeader {
            title: qsTr("Devices")
            subtitle: bluetooth && bluetooth.adapterName.length > 0
                      ? (bluetooth.adapterName + " · " + bluetooth.adapterAddress)
                      : qsTr("No Bluetooth adapter")
        }

        ErrorBanner { text: bluetooth ? bluetooth.errorText : "" }

        RowLayout {
            spacing: Metrics.sm
            Button {
                text: bluetooth && bluetooth.scanning ? qsTr("Stop Scan") : qsTr("Start Scan")
                enabled: bluetooth && (bluetooth.scanning ? bluetooth.canStopScan : bluetooth.canStartScan)
                Accessible.name: text
                onClicked: bluetooth.scanning ? bluetooth.stopScan() : bluetooth.startScan()
            }
            Button {
                text: qsTr("Refresh")
                enabled: !!bluetooth
                Accessible.name: qsTr("Refresh Bluetooth")
                onClicked: bluetooth.refresh()
            }
            Label {
                text: bluetooth && bluetooth.scanning ? qsTr("Scanning…") : qsTr("Idle")
                color: Theme.textMuted
            }
            Label {
                text: bluetooth ? qsTr("%1 devices").arg(bluetooth.deviceCount) : "0"
                color: Theme.textMuted
            }
            Item { Layout.fillWidth: true }
            TextField {
                Layout.preferredWidth: 220
                placeholderText: qsTr("Search")
                text: root.query
                onTextChanged: root.query = text
            }
        }

        RowLayout {
            Repeater {
                model: [
                    { id: "all", label: qsTr("All") },
                    { id: "nearby", label: qsTr("Nearby") },
                    { id: "known", label: qsTr("Known") },
                    { id: "connected", label: qsTr("Connected") }
                ]
                delegate: Button {
                    required property string id
                    required property string label
                    text: label
                    checkable: true
                    checked: root.filter === id
                    onClicked: root.filter = id
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Frame {
                SplitView.preferredWidth: parent.width * 0.58
                SplitView.fillHeight: true
                ListView {
                    id: deviceList
                    anchors.fill: parent
                    clip: true
                    spacing: Metrics.sm
                    model: bluetooth ? bluetooth.devices : null
                    delegate: Item {
                        id: wrap
                        objectName: "deviceWrap"
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
                        required property string buttonPolicyText
                        required property bool canControlButtons
                        required property string buttonEffectiveStatus

                        readonly property string audioStatus: audio && root.audioGraphRevision >= 0
                            ? audio.audioStatusForDevice(objectPath) : ""
                        readonly property bool matchesQuery: root.query.length === 0
                            || displayName.toLowerCase().indexOf(root.query.toLowerCase()) >= 0
                            || address.toLowerCase().indexOf(root.query.toLowerCase()) >= 0
                        readonly property bool matchesFilter: root.filter === "all"
                            || (root.filter === "nearby" && !paired)
                            || (root.filter === "known" && paired)
                            || (root.filter === "connected" && connected)

                        width: ListView.view.width
                        height: matchesQuery && matchesFilter ? row.implicitHeight : 0
                        visible: height > 0
                        clip: true

                        DeviceRow {
                            id: row
                            objectName: "deviceRow"
                            width: parent.width
                            objectPath: wrap.objectPath
                            displayName: wrap.displayName
                            address: wrap.address
                            addressType: wrap.addressType
                            transportHint: wrap.transportHint
                            hasRssi: wrap.hasRssi
                            rssi: wrap.rssi
                            paired: wrap.paired
                            connected: wrap.connected
                            trusted: wrap.trusted
                            servicesResolved: wrap.servicesResolved
                            operationText: wrap.operationText
                            lastErrorMessage: wrap.lastErrorMessage
                            canPair: wrap.canPair
                            canCancelPairing: wrap.canCancelPairing
                            canTrust: wrap.canTrust
                            canUntrust: wrap.canUntrust
                            canConnect: wrap.canConnect
                            canDisconnect: wrap.canDisconnect
                            canForget: wrap.canForget
                            canReconnect: wrap.canReconnect
                            canCancelOperation: wrap.canCancelOperation
                            uuids: wrap.uuids
                            audioStatus: wrap.audioStatus
                            buttonPolicyText: wrap.buttonPolicyText
                            canControlButtons: wrap.canControlButtons
                            buttonEffectiveStatus: wrap.buttonEffectiveStatus

                            onPairRequested: root.bluetooth.pairDevice(wrap.objectPath)
                            onCancelPairingRequested: root.bluetooth.cancelPairing(wrap.objectPath)
                            onCancelOperationRequested: root.bluetooth.cancelDeviceOperation(wrap.objectPath)
                            onTrustRequested: root.bluetooth.trustDevice(wrap.objectPath)
                            onUntrustRequested: root.bluetooth.untrustDevice(wrap.objectPath)
                            onConnectRequested: root.bluetooth.connectDevice(wrap.objectPath)
                            onDisconnectRequested: root.bluetooth.disconnectDevice(wrap.objectPath)
                            onReconnectRequested: root.bluetooth.reconnectDevice(wrap.objectPath)
                            onButtonPolicyToggled: function(disallow) {
                                root.bluetooth.setDeviceButtonPolicy(wrap.objectPath, disallow)
                            }
                            onForgetRequested: {
                                forgetDialog.devicePath = wrap.objectPath
                                forgetDialog.deviceName = wrap.displayName
                                forgetDialog.open()
                            }
                            onShowServicesRequested: root.selectedPath = wrap.objectPath
                        }

                        MouseArea {
                            anchors.fill: parent
                            z: -1
                            onClicked: root.selectedPath = wrap.objectPath
                        }
                    }

                    EmptyState {
                        visible: deviceList.count === 0
                            || (root.filter === "connected" && bluetooth && bluetooth.connectedDeviceCount === 0)
                        anchors.centerIn: parent
                        title: {
                            if (bluetooth && !bluetooth.available)
                                return qsTr("Bluetooth unavailable")
                            if (root.filter === "connected" && deviceList.count > 0)
                                return qsTr("No connected devices")
                            return bluetooth && bluetooth.scanning ? qsTr("Scanning…") : qsTr("No devices yet")
                        }
                        message: {
                            if (root.filter === "connected" && deviceList.count > 0)
                                return qsTr("Paired devices appear under All or Known. Connect them from the device row.")
                            return bluetooth && bluetooth.scanning
                                ? qsTr("Nearby devices will appear here.")
                                : qsTr("Start a scan to discover nearby Bluetooth devices.")
                        }
                    }
                }
            }

            Frame {
                SplitView.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Metrics.sm
                    Label { text: qsTr("Details"); color: Theme.text; font.bold: true }
                    Label {
                        visible: root.selectedPath.length === 0
                        text: qsTr("Select a device to inspect technical details.")
                        color: Theme.textMuted
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Label {
                        visible: root.selectedPath.length > 0 && !root.selectedAvailable
                        text: qsTr("Device no longer available")
                        color: Theme.warning
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    ScrollView {
                        visible: root.selectedAvailable
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        ColumnLayout {
                            width: parent.width
                            spacing: Metrics.sm
                            KeyValueRow { label: qsTr("Name"); value: details.name || "" }
                            KeyValueRow { label: qsTr("Alias"); value: details.alias || "" }
                            KeyValueRow { label: qsTr("Address"); value: details.address || "" }
                            KeyValueRow {
                                label: qsTr("RSSI")
                                value: details.hasRssi ? qsTr("%1 dBm").arg(details.rssi) : qsTr("Unknown")
                            }
                            KeyValueRow { label: qsTr("Paired"); value: details.paired ? qsTr("Yes") : qsTr("No") }
                            KeyValueRow { label: qsTr("Trusted"); value: details.trusted ? qsTr("Yes") : qsTr("No") }
                            KeyValueRow { label: qsTr("Connected"); value: details.connected ? qsTr("Yes") : qsTr("No") }
                            KeyValueRow { label: qsTr("Blocked"); value: details.blocked ? qsTr("Yes") : qsTr("No") }
                            KeyValueRow {
                                label: qsTr("Services resolved")
                                value: details.servicesResolved ? qsTr("Yes") : qsTr("No")
                            }
                            KeyValueRow {
                                label: qsTr("Class")
                                value: details.hasClass ? String(details.classOfDevice) : qsTr("Unknown")
                            }
                            KeyValueRow {
                                label: qsTr("Appearance")
                                value: details.hasAppearance ? String(details.appearance) : qsTr("Unknown")
                            }
                            KeyValueRow { label: qsTr("Last seen"); value: details.lastSeen || qsTr("Unknown") }
                            KeyValueRow { label: qsTr("Transport"); value: details.transport || qsTr("Unknown") }
                            KeyValueRow {
                                visible: !!audio
                                label: qsTr("Audio")
                                value: audio ? audio.audioStatusForDevice(root.selectedPath) : ""
                            }
                            KeyValueRow {
                                label: qsTr("Device buttons")
                                value: details.buttonPolicy || qsTr("ALLOW")
                            }
                            KeyValueRow {
                                visible: details.buttonPolicy === "DISALLOW"
                                label: qsTr("Button control")
                                value: details.buttonEffectiveStatus || qsTr("Unknown")
                            }
                            Label { text: qsTr("Services"); color: Theme.text; font.bold: true }
                            Repeater {
                                model: details.uuids || []
                                delegate: Label {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    wrapMode: Text.WrapAnywhere
                                    color: Theme.textMuted
                                    text: root.bluetooth.serviceFriendlyName(modelData) + " · " + modelData
                                }
                            }
                            Label {
                                visible: !details.uuids || details.uuids.length === 0
                                text: qsTr("No advertised UUIDs")
                                color: Theme.textMuted
                            }
                            CheckBox {
                                id: showPath
                                text: qsTr("Technical path")
                            }
                            Label {
                                visible: showPath.checked
                                text: root.selectedPath
                                color: Theme.textMuted
                                wrapMode: Text.WrapAnywhere
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: forgetDialog
        property string devicePath: ""
        property string deviceName: ""
        title: qsTr("Forget device")
        message: qsTr("Remove %1 from BlueZ? This cannot be undone from Auralis.").arg(deviceName)
        confirmText: qsTr("Forget")
        onConfirmed: bluetooth.forgetDevice(devicePath)
    }
}

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

                        readonly property string audioStatus: audio ? (audio.graphRevision, audio.audioStatusForDevice(objectPath)) : ""
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

                        DeviceRow {
                            id: row
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

                            onPairRequested: bluetooth.pairDevice(wrap.objectPath)
                            onCancelPairingRequested: bluetooth.cancelPairing(wrap.objectPath)
                            onCancelOperationRequested: bluetooth.cancelDeviceOperation(wrap.objectPath)
                            onTrustRequested: bluetooth.trustDevice(wrap.objectPath)
                            onUntrustRequested: bluetooth.untrustDevice(wrap.objectPath)
                            onConnectRequested: bluetooth.connectDevice(wrap.objectPath)
                            onDisconnectRequested: bluetooth.disconnectDevice(wrap.objectPath)
                            onReconnectRequested: bluetooth.reconnectDevice(wrap.objectPath)
                            onForgetRequested: {
                                forgetDialog.devicePath = wrap.objectPath
                                forgetDialog.deviceName = wrap.displayName
                                forgetDialog.open()
                            }
                            onShowServicesRequested: root.selectedPath = wrap.objectPath
                            MouseArea {
                                anchors.fill: parent
                                z: -1
                                onClicked: root.selectedPath = wrap.objectPath
                            }
                        }
                    }

                    EmptyState {
                        visible: deviceList.count === 0
                        anchors.centerIn: parent
                        title: bluetooth && !bluetooth.available ? qsTr("Bluetooth unavailable")
                             : (bluetooth && bluetooth.scanning ? qsTr("Scanning…") : qsTr("No devices yet"))
                        message: bluetooth && bluetooth.scanning
                                 ? qsTr("Nearby devices will appear here.")
                                 : qsTr("Start a scan to discover nearby Bluetooth devices.")
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
                        visible: root.selectedPath.length > 0
                        text: root.selectedPath
                        color: Theme.textMuted
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                    Label {
                        visible: root.selectedPath.length > 0 && audio
                        text: qsTr("Audio: %1").arg(audio.audioStatusForDevice(root.selectedPath))
                        color: Theme.text
                    }
                    Item { Layout.fillHeight: true }
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

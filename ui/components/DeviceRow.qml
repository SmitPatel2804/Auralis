import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Column {
    id: root

    property string displayName
    property string address
    property string addressType
    property string transportHint
    property bool hasRssi: false
    property int rssi: 0
    property bool paired: false
    property bool connected: false
    property bool trusted: false
    property bool servicesResolved: false
    property string objectPath
    property string operationText: ""
    property string lastErrorMessage: ""
    property bool canPair: false
    property bool canCancelPairing: false
    property bool canTrust: false
    property bool canUntrust: false
    property bool canConnect: false
    property bool canDisconnect: false
    property bool canForget: false
    property bool canReconnect: false
    property var uuids: []

    signal pairRequested()
    signal cancelPairingRequested()
    signal trustRequested()
    signal untrustRequested()
    signal connectRequested()
    signal disconnectRequested()
    signal forgetRequested()
    signal reconnectRequested()
    signal showServicesRequested()

    spacing: 6

    Text {
        text: root.displayName
        font.pixelSize: 16
        font.bold: true
    }

    Text {
        text: root.address.length > 0 ? root.address : "Address unknown"
        font.pixelSize: 13
        color: "#444444"
    }

    Text {
        text: (root.addressType.length > 0 ? root.addressType : "type unknown")
              + " / " + (root.transportHint.length > 0 ? root.transportHint : "Unknown")
        font.pixelSize: 13
        color: "#444444"
    }

    Text {
        text: root.hasRssi ? ("RSSI: " + root.rssi + " dBm") : "Signal: Unknown"
        font.pixelSize: 13
        color: "#444444"
    }

    Text {
        visible: root.paired || root.connected || root.trusted || root.servicesResolved
        text: (root.paired ? "Paired" : "")
              + (root.paired && (root.connected || root.trusted || root.servicesResolved) ? " · " : "")
              + (root.connected ? "Connected" : "")
              + (root.connected && (root.trusted || root.servicesResolved) ? " · " : "")
              + (root.trusted ? "Trusted" : "")
              + ((root.trusted || root.connected || root.paired) && root.servicesResolved ? " · " : "")
              + (root.servicesResolved ? "Services resolved" : "")
        font.pixelSize: 12
        color: "#666666"
    }

    Text {
        visible: root.operationText.length > 0
        text: root.operationText
        font.pixelSize: 12
        color: "#1565c0"
    }

    Text {
        visible: root.lastErrorMessage.length > 0
        text: root.lastErrorMessage
        font.pixelSize: 12
        color: "#c62828"
        wrapMode: Text.WordWrap
        width: parent.width
    }

    Flow {
        width: parent.width
        spacing: 8

        Button {
            text: "Pair"
            visible: root.canPair
            onClicked: root.pairRequested()
        }
        Button {
            text: "Cancel"
            visible: root.canCancelPairing
            onClicked: root.cancelPairingRequested()
        }
        Button {
            text: "Trust"
            visible: root.canTrust
            onClicked: root.trustRequested()
        }
        Button {
            text: "Untrust"
            visible: root.canUntrust
            onClicked: root.untrustRequested()
        }
        Button {
            text: "Connect"
            visible: root.canConnect
            onClicked: root.connectRequested()
        }
        Button {
            text: "Disconnect"
            visible: root.canDisconnect
            onClicked: root.disconnectRequested()
        }
        Button {
            text: "Reconnect"
            visible: root.canReconnect
            onClicked: root.reconnectRequested()
        }
        Button {
            text: "Forget"
            visible: root.canForget
            onClicked: root.forgetRequested()
        }
        Button {
            text: "Services"
            visible: root.uuids.length > 0
            onClicked: root.showServicesRequested()
        }
    }
}

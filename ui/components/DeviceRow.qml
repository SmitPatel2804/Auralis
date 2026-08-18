import QtQuick

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
    property bool servicesResolved: false

    spacing: 4

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
        visible: root.paired || root.connected || root.servicesResolved
        text: (root.paired ? "Paired" : "")
              + (root.paired && (root.connected || root.servicesResolved) ? " · " : "")
              + (root.connected ? "Connected" : "")
              + (root.connected && root.servicesResolved ? " · " : "")
              + (root.servicesResolved ? "Services resolved" : "")
        font.pixelSize: 12
        color: "#666666"
    }
}

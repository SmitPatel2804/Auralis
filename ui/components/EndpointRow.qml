import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Column {
    id: root

    property string name
    property string direction
    property bool available: false
    property string transport
    property string profile
    property string codec
    property bool mapped: false
    property string bluetoothDisplayName
    property string bluetoothAddress
    property int pipeWireObjectId: 0
    property bool showDeveloperDetail: false

    spacing: 4
    width: parent != null ? parent.width : implicitWidth

    Text {
        text: root.name
        font.pixelSize: 15
        font.bold: true
        color: Theme.text
        wrapMode: Text.WordWrap
        width: parent.width
    }

    Text {
        text: root.direction
              + " · " + (root.available ? "Available" : "Unavailable")
              + " · " + (root.transport.length > 0 ? root.transport : "Unknown")
              + (root.profile.length > 0 ? (" · " + root.profile) : "")
        font.pixelSize: 12
        color: Theme.textMuted
        wrapMode: Text.WordWrap
        width: parent.width
    }

    Text {
        visible: root.mapped && root.bluetoothDisplayName.length > 0
        text: "Mapped: " + root.bluetoothDisplayName
        font.pixelSize: 12
        color: Theme.accent
    }

    Text {
        visible: root.showDeveloperDetail
        text: qsTr("Audio node ID: %1").arg(root.pipeWireObjectId)
              + (root.bluetoothAddress.length > 0 ? (" · " + root.bluetoothAddress) : "")
              + (root.codec.length > 0 ? (" · " + root.codec) : "")
        font.pixelSize: 11
        color: Theme.textMuted
    }
}

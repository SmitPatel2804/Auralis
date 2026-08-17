import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string label
    property string status
    property bool healthy: status === "Ready"

    spacing: 24

    Text {
        text: root.label
        font.pixelSize: 16
        Layout.preferredWidth: 160
    }

    Text {
        text: root.status
        font.pixelSize: 16
        color: root.healthy ? "#2e7d32" : "#6d4c41"
    }
}

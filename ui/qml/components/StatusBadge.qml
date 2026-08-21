import QtQuick
import QtQuick.Controls

Label {
    id: root
    property string kind: "idle"
    property string label: ""

    text: "  " + root.label.toUpperCase()
    color: Theme.statusColor(root.kind)
    font.pixelSize: 9
    font.bold: true
    font.letterSpacing: 0.55
    leftPadding: 9
    rightPadding: 10
    topPadding: 5
    bottomPadding: 5
    background: Rectangle {
        radius: 999
        color: Theme.alpha(Theme.statusColor(root.kind), 0.11)
        border.color: Theme.alpha(Theme.statusColor(root.kind), 0.48)
        border.width: 1

        Rectangle {
            width: 5
            height: 5
            radius: 3
            x: 7
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.statusColor(root.kind)
        }
    }
}

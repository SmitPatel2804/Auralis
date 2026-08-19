import QtQuick
import QtQuick.Controls

Label {
    id: root
    property string kind: "idle"
    property string label: ""

    text: root.label
    color: Theme.statusColor(root.kind)
    font.pixelSize: Metrics.caption
    font.bold: true
    leftPadding: 8
    rightPadding: 8
    topPadding: 3
    bottomPadding: 3
    background: Rectangle {
        radius: 999
        color: Qt.rgba(Theme.statusColor(root.kind).r, Theme.statusColor(root.kind).g, Theme.statusColor(root.kind).b, 0.16)
        border.color: Theme.statusColor(root.kind)
        border.width: 1
    }
}

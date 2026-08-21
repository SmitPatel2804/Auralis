import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string text: ""
    property string kind: "error"
    visible: root.text.length > 0
    color: Theme.alpha(Theme.statusColor(root.kind), 0.1)
    border.color: Theme.alpha(Theme.statusColor(root.kind), 0.58)
    radius: Theme.cardRadius
    implicitHeight: label.implicitHeight + Metrics.md * 2
    Layout.fillWidth: true

    Label {
        id: label
        anchors.fill: parent
        anchors.margins: Metrics.md
        text: "!  " + root.text
        color: Theme.text
        wrapMode: Text.WordWrap
        font.pixelSize: 13
    }
}

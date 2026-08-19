import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string text: ""
    property string kind: "error"
    visible: root.text.length > 0
    color: Qt.rgba(Theme.statusColor(root.kind).r, Theme.statusColor(root.kind).g, Theme.statusColor(root.kind).b, 0.12)
    border.color: Theme.statusColor(root.kind)
    radius: Theme.radius
    implicitHeight: label.implicitHeight + Metrics.sm * 2
    Layout.fillWidth: true

    Label {
        id: label
        anchors.fill: parent
        anchors.margins: Metrics.sm
        text: root.text
        color: Theme.text
        wrapMode: Text.WordWrap
    }
}

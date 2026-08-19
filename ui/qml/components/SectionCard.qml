import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string title: ""
    default property alias content: body.data

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border
    border.width: 1
    implicitHeight: column.implicitHeight + Metrics.md * 2
    Layout.fillWidth: true

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Metrics.md
        spacing: Metrics.sm

        Label {
            visible: root.title.length > 0
            text: root.title
            color: Theme.text
            font.pixelSize: 16
            font.bold: true
            Layout.fillWidth: true
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: Metrics.sm
        }
    }
}

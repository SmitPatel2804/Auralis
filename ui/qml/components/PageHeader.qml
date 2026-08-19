import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    property string title: ""
    property string subtitle: ""
    Layout.fillWidth: true
    spacing: Metrics.sm

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2
        Label {
            text: root.title
            color: Theme.text
            font.pixelSize: Metrics.pageHeader
            font.bold: true
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: Theme.textMuted
            font.pixelSize: Metrics.caption
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string title: ""
    property string message: ""
    property string actionText: ""
    signal actionRequested()

    spacing: Metrics.sm
    Layout.fillWidth: true

    Label {
        text: root.title
        color: Theme.text
        font.pixelSize: 16
        font.bold: true
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }
    Label {
        visible: root.message.length > 0
        text: root.message
        color: Theme.textMuted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Button {
        visible: root.actionText.length > 0
        text: root.actionText
        Accessible.name: root.actionText
        onClicked: root.actionRequested()
    }
}

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

    Rectangle {
        Layout.preferredWidth: 42
        Layout.preferredHeight: 42
        radius: 14
        color: Theme.alpha(Theme.accent, 0.08)
        border.color: Theme.alpha(Theme.accent, 0.32)
        Label {
            anchors.centerIn: parent
            text: "//"
            color: Theme.accent
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 1
        }
    }

    Label {
        text: root.title
        color: Theme.text
        font.pixelSize: 17
        font.weight: Font.DemiBold
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
    SignalButton {
        visible: root.actionText.length > 0
        text: root.actionText
        primary: true
        Accessible.name: root.actionText
        onClicked: root.actionRequested()
    }
}

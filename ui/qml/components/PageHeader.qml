import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    property string title: ""
    property string subtitle: ""
    Layout.fillWidth: true
    spacing: Metrics.md

    Rectangle {
        Layout.preferredWidth: 4
        Layout.preferredHeight: 48
        radius: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.accent }
            GradientStop { position: 1.0; color: Theme.accentSecondary }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 3
        Label {
            text: qsTr("AURALIS // COMMAND SURFACE")
            color: Theme.accent
            font.pixelSize: 9
            font.bold: true
            font.letterSpacing: 1.4
            Layout.fillWidth: true
        }
        Label {
            text: root.title
            color: Theme.text
            font.pixelSize: Metrics.pageHeader
            font.weight: Font.DemiBold
            font.letterSpacing: 0.2
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: Theme.textMuted
            font.pixelSize: Metrics.caption
            font.letterSpacing: 0.15
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}

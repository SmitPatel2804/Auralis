import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string eyebrow: ""
    property string value: ""
    property string detail: ""
    property string glyph: "00"
    property color signalColor: Theme.accent

    Layout.fillWidth: true
    implicitHeight: 116
    radius: Theme.cardRadius
    border.color: Theme.alpha(root.signalColor, 0.32)
    border.width: 1
    gradient: Gradient {
        GradientStop { position: 0.0; color: Theme.alpha(root.signalColor, 0.12) }
        GradientStop { position: 0.42; color: Theme.surfaceAlt }
        GradientStop { position: 1.0; color: Theme.surface }
    }

    Rectangle {
        width: 3
        height: parent.height - Metrics.md * 2
        radius: 2
        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.verticalCenter: parent.verticalCenter
        color: root.signalColor
        opacity: 0.82
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Metrics.md
        spacing: Metrics.md

        Rectangle {
            Layout.preferredWidth: 50
            Layout.preferredHeight: 50
            radius: 15
            color: Theme.alpha(root.signalColor, 0.13)
            border.color: Theme.alpha(root.signalColor, 0.4)

            Label {
                anchors.centerIn: parent
                text: root.glyph
                color: root.signalColor
                font.pixelSize: 13
                font.bold: true
                font.letterSpacing: 1.4
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Label {
                text: root.eyebrow.toUpperCase()
                color: Theme.textMuted
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.35
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Label {
                text: root.value
                color: Theme.text
                font.pixelSize: 23
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Label {
                text: root.detail
                color: root.signalColor
                font.pixelSize: Metrics.caption
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
    }
}

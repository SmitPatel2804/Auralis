import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string title: ""
    property string subtitle: ""
    property color signalColor: Theme.accent
    default property alias content: body.data

    radius: Theme.cardRadius
    border.color: Theme.alpha(root.signalColor, 0.23)
    border.width: 1
    implicitHeight: column.implicitHeight + Metrics.md * 2
    Layout.fillWidth: true
    gradient: Gradient {
        GradientStop { position: 0.0; color: Theme.alpha(root.signalColor, 0.065) }
        GradientStop { position: 0.22; color: Theme.surfaceAlt }
        GradientStop { position: 1.0; color: Theme.surface }
    }

    Rectangle {
        visible: root.title.length > 0
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: Theme.cardRadius
        width: Math.min(parent.width * 0.32, 150)
        height: 2
        radius: 1
        color: root.signalColor
        opacity: 0.78
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Metrics.md
        spacing: Metrics.sm

        ColumnLayout {
            visible: root.title.length > 0
            Layout.fillWidth: true
            spacing: 2

            Label {
                text: root.title
                color: Theme.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
                font.letterSpacing: 0.25
                Layout.fillWidth: true
            }
            Label {
                visible: root.subtitle.length > 0
                text: root.subtitle
                color: Theme.textMuted
                font.pixelSize: 11
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: Metrics.sm
        }
    }
}

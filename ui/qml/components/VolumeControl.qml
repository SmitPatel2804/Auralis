import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    property string label: qsTr("Volume")
    property real value: 1
    property bool muted: false
    property bool volumeCapable: true
    signal volumeCommitted(real value)
    signal muteToggled(bool muted)

    spacing: Metrics.sm
    Layout.fillWidth: true
    enabled: root.volumeCapable
    opacity: enabled ? 1 : Theme.disabledOpacity

    Label {
        text: root.label
        color: Theme.text
        Accessible.name: root.label
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        from: 0
        to: 1
        value: root.value
        Accessible.name: root.label
        onPressedChanged: {
            if (!pressed)
                root.volumeCommitted(value)
        }
        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 5
            width: slider.availableWidth
            height: implicitHeight
            radius: 3
            color: Theme.surfaceRaised
            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 3
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.accentSecondary }
                    GradientStop { position: 1.0; color: Theme.accent }
                }
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 17
            implicitHeight: 17
            radius: 9
            color: Theme.text
            border.color: Theme.accent
            border.width: 3
        }
    }
    Label {
        text: Math.round(slider.value * 100) + "%"
        color: Theme.textMuted
        Layout.preferredWidth: 44
    }
    CheckBox {
        text: qsTr("Mute")
        checked: root.muted
        Accessible.name: qsTr("Mute")
        onToggled: root.muteToggled(checked)
    }
}

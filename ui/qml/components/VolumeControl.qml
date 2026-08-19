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

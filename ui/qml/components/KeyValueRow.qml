import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    property string label: ""
    property string value: ""
    Layout.fillWidth: true
    spacing: Metrics.md
    implicitHeight: Math.max(26, Math.max(keyLabel.implicitHeight, valueLabel.implicitHeight))

    Label {
        id: keyLabel
        text: root.label
        color: Theme.textMuted
        Layout.preferredWidth: 140
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        font.letterSpacing: 0.2
    }
    Label {
        id: valueLabel
        text: root.value.length > 0 ? root.value : "—"
        color: Theme.text
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
        font.pixelSize: 13
        font.weight: Font.Medium
    }
}

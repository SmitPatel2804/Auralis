import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    property string label: ""
    property string value: ""
    Layout.fillWidth: true
    spacing: Metrics.md

    Label {
        text: root.label
        color: Theme.textMuted
        Layout.preferredWidth: 140
        wrapMode: Text.WordWrap
    }
    Label {
        text: root.value.length > 0 ? root.value : "—"
        color: Theme.text
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
    }
}

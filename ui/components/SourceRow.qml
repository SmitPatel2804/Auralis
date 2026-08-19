import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string name
    property string sourceType
    property string applicationName
    property bool monitorSource: false
    property bool available: true

    spacing: 8
    width: parent != null ? parent.width : implicitWidth

    Text {
        text: root.name
        font.pixelSize: 14
        elide: Text.ElideRight
        color: Theme.text
        Layout.fillWidth: true
    }

    Text {
        text: root.applicationName.length > 0
              ? root.applicationName
              : (root.monitorSource ? "Monitor" : root.sourceType)
        font.pixelSize: 11
        color: Theme.textMuted
        elide: Text.ElideRight
        Layout.preferredWidth: 140
    }
}

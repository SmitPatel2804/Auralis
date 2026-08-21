import QtQuick
import Auralis 1.0
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var model: null
    implicitHeight: column.implicitHeight
    Layout.fillWidth: true

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Metrics.xs

        Repeater {
            model: root.model
            delegate: Rectangle {
                required property string notificationId
                required property string severity
                required property string title
                required property string message
                Layout.fillWidth: true
                radius: Theme.cardRadius
                color: Theme.surfaceRaised
                border.color: Theme.alpha(Theme.statusColor(severity === "error" ? "error" : (severity === "warning" ? "warning" : "info")), 0.7)
                implicitHeight: inner.implicitHeight + Metrics.sm * 2

                RowLayout {
                    id: inner
                    anchors.fill: parent
                    anchors.margins: Metrics.sm
                    spacing: Metrics.sm

                    Label {
                        text: title.length > 0 ? (title + ": " + message) : message
                        color: Theme.text
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Button {
                        text: qsTr("Dismiss")
                        Accessible.name: qsTr("Dismiss notification")
                        onClicked: AppCore.notifications.dismiss(notificationId)
                    }
                }
            }
        }
    }
}

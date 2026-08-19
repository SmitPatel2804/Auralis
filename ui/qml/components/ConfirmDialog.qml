import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    modal: true
    focus: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    title: qsTr("Confirm")
    property string message: ""
    property string confirmText: qsTr("Confirm")
    signal confirmed()

    standardButtons: Dialog.NoButton

    contentItem: Label {
        text: root.message
        wrapMode: Text.WordWrap
        color: Theme.text
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.reject()
        }
        Button {
            text: root.confirmText
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: {
                root.confirmed()
                root.accept()
            }
        }
    }
}

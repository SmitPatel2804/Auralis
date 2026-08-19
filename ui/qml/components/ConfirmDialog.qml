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
    readonly property int dialogWidth: 420
    signal confirmed()

    width: dialogWidth
    standardButtons: Dialog.NoButton

    contentItem: Label {
        width: root.dialogWidth - root.leftPadding - root.rightPadding
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        text: root.message
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

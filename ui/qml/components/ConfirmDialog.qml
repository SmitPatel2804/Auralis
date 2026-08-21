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
    background: Rectangle {
        radius: Theme.cardRadius
        color: Theme.surfaceAlt
        border.color: Theme.alpha(Theme.accent, 0.48)
    }

    contentItem: Label {
        width: root.dialogWidth - root.leftPadding - root.rightPadding
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        text: root.message
        color: Theme.text
    }

    footer: DialogButtonBox {
        SignalButton {
            text: qsTr("CANCEL")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.reject()
        }
        SignalButton {
            text: root.confirmText
            primary: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: {
                root.confirmed()
                root.accept()
            }
        }
    }
}

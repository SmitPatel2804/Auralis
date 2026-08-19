import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

Dialog {
    id: root
    modal: true
    focus: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.NoButton
    title: "Bluetooth Pairing"
    implicitWidth: 440
    implicitHeight: contentColumn.implicitHeight + 48

    property var request: null
    property var bluetooth: null

    readonly property bool needsInput: request && request.needsInput
    readonly property bool needsConfirmation: request && request.needsConfirmation
    readonly property string trimmedPin: pinField.text.trim()
    readonly property string trimmedPasskey: passkeyField.text.trim()
    readonly property bool pinValid: !needsInput || !request || request.requestType !== Pairing.EnterPin
        || (trimmedPin.length > 0 && trimmedPin.length <= 16)
    readonly property bool passkeyValid: !needsInput || !request || request.requestType !== Pairing.EnterPasskey
        || (/^\d{1,6}$/.test(trimmedPasskey))

    onRequestChanged: {
        pinField.text = ""
        passkeyField.text = ""
        if (request) {
            open()
        } else {
            close()
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: 12

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: request ? (request.deviceName.length > 0 ? request.deviceName : request.devicePath) : ""
            font.pixelSize: 16
            font.bold: true
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            visible: request && request.message.length > 0
            text: request ? request.message : ""
        }

        Text {
            Layout.fillWidth: true
            visible: request && request.pinCode.length > 0
            text: request ? ("PIN: " + request.pinCode) : ""
            font.pixelSize: 14
        }

        Text {
            Layout.fillWidth: true
            visible: request && request.passkey > 0
            text: request ? ("Passkey: " + ("000000" + request.passkey).slice(-6)) : ""
            font.family: "monospace"
            font.pixelSize: 18
        }

        TextField {
            id: pinField
            Layout.fillWidth: true
            visible: needsInput && request && request.requestType === Pairing.EnterPin
            placeholderText: "Enter PIN"
            echoMode: TextInput.Password
        }

        TextField {
            id: passkeyField
            Layout.fillWidth: true
            visible: needsInput && request && request.requestType === Pairing.EnterPasskey
            placeholderText: "Enter passkey"
            inputMethodHints: Qt.ImhDigitsOnly
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: needsInput ? "Submit" : "Accept"
                enabled: request && bluetooth && pinValid && passkeyValid
                onClicked: {
                    if (!request || !bluetooth) {
                        return
                    }
                    if (needsInput && request.requestType === Pairing.EnterPin) {
                        bluetooth.submitPinCode(request.requestId, trimmedPin)
                    } else if (needsInput && request.requestType === Pairing.EnterPasskey) {
                        bluetooth.submitPasskey(request.requestId, Number(trimmedPasskey))
                    } else {
                        bluetooth.acceptPairingRequest(request.requestId)
                    }
                }
            }

            Button {
                text: "Reject"
                enabled: request && bluetooth
                onClicked: {
                    if (request && bluetooth) {
                        bluetooth.rejectPairingRequest(request.requestId)
                    }
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string displayName
    property string address
    property string addressType
    property string transportHint
    property bool hasRssi: false
    property int rssi: 0
    property bool paired: false
    property bool connected: false
    property bool trusted: false
    property bool servicesResolved: false
    property string objectPath
    property string operationText: ""
    property string lastErrorMessage: ""
    property bool canPair: false
    property bool canCancelPairing: false
    property bool canTrust: false
    property bool canUntrust: false
    property bool canConnect: false
    property bool canDisconnect: false
    property bool canForget: false
    property bool canReconnect: false
    property bool canCancelOperation: false
    property var uuids: []
    property string audioStatus: ""
    property string buttonPolicyText: "ALLOW"
    property bool canControlButtons: false
    property string buttonEffectiveStatus: ""

    signal pairRequested()
    signal cancelPairingRequested()
    signal cancelOperationRequested()
    signal trustRequested()
    signal untrustRequested()
    signal connectRequested()
    signal disconnectRequested()
    signal forgetRequested()
    signal reconnectRequested()
    signal showServicesRequested()
    signal buttonPolicyToggled(bool disallow)
    signal manageInOsRequested()
    signal manageInAppRequested()

    readonly property string signalText: root.hasRssi
        ? (root.rssi + " dBm")
        : (root.connected
           ? qsTr("LINK ACTIVE · RSSI N/A")
           : (root.paired
              ? qsTr("PAIRED · RSSI N/A")
              : (root.transportHint === "BLE" ? qsTr("AWAITING ADVERTISEMENT") : qsTr("RSSI UNAVAILABLE"))))

    implicitHeight: content.implicitHeight + Metrics.md * 2
    radius: Theme.cardRadius
    border.width: 1
    border.color: Theme.alpha(root.connected ? Theme.success : (root.paired ? Theme.accent : Theme.borderBright), 0.38)
    gradient: Gradient {
        GradientStop { position: 0.0; color: Theme.alpha(root.connected ? Theme.success : Theme.accent, root.connected ? 0.085 : 0.04) }
        GradientStop { position: 0.3; color: Theme.surfaceAlt }
        GradientStop { position: 1.0; color: Theme.surface }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        radius: 2
        color: root.connected ? Theme.success : (root.paired ? Theme.accent : Theme.textFaint)
        opacity: root.connected || root.paired ? 0.9 : 0.35
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Metrics.md
        spacing: Metrics.sm

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.sm

            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                radius: 14
                color: Theme.alpha(root.connected ? Theme.success : Theme.accent, 0.11)
                border.color: Theme.alpha(root.connected ? Theme.success : Theme.accent, 0.42)
                Label {
                    anchors.centerIn: parent
                    text: "BT"
                    color: root.connected ? Theme.success : Theme.accent
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    text: root.displayName
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: Theme.text
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Label {
                    text: root.address.length > 0 ? root.address : qsTr("Address unavailable")
                    font.pixelSize: 11
                    font.letterSpacing: 0.55
                    color: Theme.textMuted
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            StatusBadge {
                label: root.connected ? qsTr("Connected") : (root.paired ? qsTr("Paired") : qsTr("Nearby"))
                kind: root.connected ? "connected" : (root.paired ? "busy" : "idle")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.md

            Label {
                text: (root.addressType.length > 0 ? root.addressType : qsTr("Unknown type"))
                      + "  //  " + (root.transportHint.length > 0 ? root.transportHint : qsTr("Unknown transport"))
                color: Theme.textMuted
                font.pixelSize: 10
                font.letterSpacing: 0.45
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            RowLayout {
                spacing: 3
                Repeater {
                    model: 4
                    Rectangle {
                        required property int index
                        Layout.preferredWidth: 4
                        Layout.preferredHeight: 6 + index * 3
                        radius: 2
                        color: root.hasRssi && root.rssi >= (-90 + index * 12)
                               ? Theme.accent
                               : Theme.alpha(Theme.textMuted, 0.22)
                    }
                }
                Label {
                    text: root.signalText
                    font.pixelSize: 9
                    color: root.hasRssi || root.connected ? Theme.accent : Theme.textFaint
                    Layout.leftMargin: 3
                }
            }
        }

        RowLayout {
            visible: root.trusted || root.servicesResolved || root.audioStatus.length > 0 || root.operationText.length > 0
            Layout.fillWidth: true
            spacing: Metrics.xs

            StatusBadge { visible: root.trusted; label: qsTr("Trusted"); kind: "ready" }
            StatusBadge { visible: root.servicesResolved; label: qsTr("Services ready"); kind: "ready" }
            StatusBadge {
                visible: root.audioStatus.length > 0
                label: qsTr("Audio %1").arg(root.audioStatus)
                kind: root.audioStatus === "Available" ? "ready" : "busy"
            }
            Label {
                visible: root.operationText.length > 0
                text: root.operationText.toUpperCase()
                color: Theme.accent
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 0.6
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Item { Layout.fillWidth: true }
        }

        Label {
            visible: root.lastErrorMessage.length > 0
            text: "!  " + root.lastErrorMessage
            color: Theme.danger
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            visible: root.paired || root.connected
            Layout.fillWidth: true
            spacing: Metrics.sm
            Label {
                text: qsTr("Device buttons: %1").arg(root.buttonPolicyText)
                font.pixelSize: 11
                color: Theme.textMuted
                Layout.fillWidth: true
            }
            SignalButton {
                text: root.buttonPolicyText === "DISALLOW" ? qsTr("ALLOW") : qsTr("DISALLOW")
                compact: true
                enabled: root.canControlButtons
                ToolTip.visible: hovered && !enabled
                ToolTip.text: qsTr("Per-device enforcement is unavailable on this operating system")
                onClicked: root.buttonPolicyToggled(root.buttonPolicyText !== "DISALLOW")
            }
        }
        Label {
            visible: (root.buttonPolicyText === "DISALLOW" && root.buttonEffectiveStatus.length > 0)
                || (!root.canControlButtons && (root.paired || root.connected))
            text: root.buttonEffectiveStatus
            font.pixelSize: 11
            color: Theme.textMuted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Flow {
            id: actions
            Layout.fillWidth: true
            Layout.preferredHeight: childrenRect.height
            spacing: 7

            SignalButton { text: qsTr("PAIR"); compact: true; primary: true; visible: root.canPair; onClicked: root.pairRequested() }
            SignalButton { text: qsTr("CANCEL"); compact: true; visible: root.canCancelPairing; onClicked: root.cancelPairingRequested() }
            SignalButton { text: qsTr("STOP"); compact: true; danger: true; visible: root.canCancelOperation; onClicked: root.cancelOperationRequested() }
            SignalButton { text: qsTr("TRUST"); compact: true; visible: root.canTrust; onClicked: root.trustRequested() }
            SignalButton { text: qsTr("UNTRUST"); compact: true; visible: root.canUntrust; onClicked: root.untrustRequested() }
            SignalButton { text: qsTr("CONNECT"); compact: true; primary: true; visible: root.canConnect; onClicked: root.connectRequested() }
            SignalButton { text: qsTr("DISCONNECT"); compact: true; visible: root.canDisconnect; onClicked: root.disconnectRequested() }
            SignalButton {
                text: qsTr("MANAGE IN APP")
                compact: true
                primary: true
                visible: root.paired || root.connected
                onClicked: root.manageInAppRequested()
            }
            SignalButton {
                text: qsTr("MANAGE IN OS")
                compact: true
                visible: root.connected && !root.canDisconnect
                    && (Qt.platform.os === "windows" || Qt.platform.os === "osx")
                onClicked: root.manageInOsRequested()
            }
            SignalButton { text: qsTr("RECONNECT"); compact: true; visible: root.canReconnect; onClicked: root.reconnectRequested() }
            SignalButton { text: qsTr("FORGET"); compact: true; danger: true; visible: root.canForget; onClicked: root.forgetRequested() }
            SignalButton { text: qsTr("SERVICES"); compact: true; visible: root.uuids.length > 0; onClicked: root.showServicesRequested() }
        }
    }
}

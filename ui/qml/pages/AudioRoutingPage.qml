import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

Flickable {
    id: root
    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight
    readonly property var audio: AppCore.audio
    readonly property var router: audio ? audio.router : null
    property string pendingRemovalRouteId: ""

    ColumnLayout {
        id: column
        width: root.width
        spacing: Metrics.md

        PageHeader { title: qsTr("Signal Graph"); subtitle: qsTr("Sources, destinations and active native audio routes") }

        ErrorBanner { text: router ? router.lastErrorText : "" }

        SectionCard {
            visible: Qt.platform.os === "windows"
            title: qsTr("Windows capture mode")
            subtitle: qsTr("Safe copy-mode routing")
            signalColor: Theme.warning
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textMuted
                text: qsTr("Application capture is currently a copy of Windows playback. Auralis blocks routing that copy back into the same Windows default output because it causes delayed double audio. Select another Windows output or remove that default device from the Auralis route. A signed Auralis Virtual Output driver is the planned exclusive-routing mode.")
            }
        }

        SectionCard {
            title: qsTr("Sources")
            subtitle: qsTr("Capture nodes available to the routing engine")
            signalColor: Theme.accent
            Repeater {
                model: router ? router.sources : null
                delegate: RowLayout {
                    required property string name
                    required property string sourceType
                    required property bool available
                    Layout.fillWidth: true
                    Label { text: name; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                    StatusBadge { label: available ? qsTr("Available") : qsTr("Unavailable"); kind: available ? "ready" : "warning" }
                    Label { text: sourceType; color: Theme.textMuted; font.pixelSize: 12 }
                }
            }
            EmptyState {
                visible: !router || router.sourceCount === 0
                title: qsTr("No sources")
                message: qsTr("Play audio or wait for a capture source.")
            }
        }

        SectionCard {
            title: qsTr("Endpoints")
            subtitle: qsTr("Playback destinations correlated with your devices")
            signalColor: Theme.accentSecondary
            Repeater {
                model: audio ? audio.endpoints : null
                delegate: Item {
                    required property string name
                    required property string direction
                    required property bool available
                    required property string transport
                    required property string profile
                    required property string codec
                    required property bool mapped
                    required property string bluetoothDisplayName
                    required property string bluetoothAddress
                    required property int pipeWireObjectId
                    width: parent ? parent.width : 0
                    height: ep.implicitHeight
                    EndpointRow {
                        id: ep
                        width: parent.width
                        name: parent.name
                        direction: parent.direction
                        available: parent.available
                        transport: parent.transport
                        profile: parent.profile
                        codec: parent.codec
                        mapped: parent.mapped
                        bluetoothDisplayName: parent.bluetoothDisplayName
                        bluetoothAddress: parent.bluetoothAddress
                        pipeWireObjectId: parent.pipeWireObjectId
                        showDeveloperDetail: AppCore.showDeveloperStatus
                    }
                }
            }
        }

        SectionCard {
            title: qsTr("Routes")
            subtitle: qsTr("Owned signal paths and activation state")
            signalColor: Theme.success
            Repeater {
                model: router ? router.routes : null
                delegate: ColumnLayout {
                    required property string routeId
                    required property string sourceName
                    required property int destinationCount
                    required property string stateText
                    required property string errorText
                    required property bool active
                    required property bool editable
                    required property string ownerLabel
                    Layout.fillWidth: true
                    RowLayout {
                        Label { text: sourceName; color: Theme.text; font.bold: true; Layout.fillWidth: true }
                        StatusBadge { label: ownerLabel; kind: editable ? "idle" : "busy" }
                        StatusBadge { label: stateText; kind: active ? "active" : (stateText === "Failed" ? "error" : "idle") }
                    }
                    Label { text: qsTr("%1 destinations").arg(destinationCount); color: Theme.textMuted }
                    Label { visible: errorText.length > 0; text: errorText; color: Theme.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    RowLayout {
                        SignalButton { text: qsTr("ACTIVATE"); primary: true; compact: true; enabled: editable; onClicked: root.router.activateRoute(routeId) }
                        SignalButton { text: qsTr("DEACTIVATE"); compact: true; enabled: editable; onClicked: root.router.deactivateRoute(routeId) }
                        SignalButton {
                            text: qsTr("REMOVE")
                            compact: true
                            danger: true
                            enabled: editable
                            onClicked: {
                                root.pendingRemovalRouteId = routeId
                                removeRouteDialog.open()
                            }
                        }
                    }
                }
            }
            EmptyState {
                visible: !router || (router.routes && router.routes.rowCount() === 0)
                title: qsTr("No routes yet")
                message: qsTr("Use the planner below or activate a session.")
            }
        }

        RoutePanel {
            Layout.fillWidth: true
            audio: root.audio
            showDeveloperDetail: AppCore.showDeveloperStatus
        }
    }

    ConfirmDialog {
        id: removeRouteDialog
        title: qsTr("Remove route")
        message: qsTr("Remove this manual signal route? Any active links owned by it will be stopped.")
        confirmText: qsTr("Remove")
        onConfirmed: {
            if (root.router && root.pendingRemovalRouteId.length > 0)
                root.router.removeRoute(root.pendingRemovalRouteId)
            root.pendingRemovalRouteId = ""
        }
        onRejected: root.pendingRemovalRouteId = ""
    }
}

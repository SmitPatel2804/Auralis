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

    ColumnLayout {
        id: column
        width: root.width
        spacing: Metrics.md

        PageHeader { title: qsTr("Audio Routing"); subtitle: qsTr("Sources, destinations, and active routes") }

        ErrorBanner { text: router ? router.lastErrorText : "" }

        SectionCard {
            title: qsTr("Sources")
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
                        Button { text: qsTr("Activate"); enabled: editable; onClicked: root.router.activateRoute(routeId) }
                        Button { text: qsTr("Deactivate"); enabled: editable; onClicked: root.router.deactivateRoute(routeId) }
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
}

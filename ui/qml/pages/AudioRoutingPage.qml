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
    readonly property bool linuxAudio: Qt.platform.os === "linux"
    readonly property bool windowsAudio: Qt.platform.os === "windows"
    property string pendingRemovalRouteId: ""

    ColumnLayout {
        id: column
        width: root.width
        spacing: Metrics.md

        PageHeader { title: qsTr("Signal Graph"); subtitle: qsTr("Sources, destinations and active native audio routes") }

        ErrorBanner { text: router ? router.lastErrorText : "" }

        SectionCard {
            visible: root.windowsAudio || root.linuxAudio
            title: qsTr("Auralis Virtual Output")
            subtitle: audio && audio.virtualOutputAvailable
                      ? (audio.virtualOutputSelected ? qsTr("Active system-audio path")
                                                     : qsTr("Ready - select it as the system output"))
                      : (root.linuxAudio ? qsTr("Creating the PipeWire virtual output")
                                         : qsTr("Signed driver required"))
            signalColor: audio && audio.virtualOutputSelected ? Theme.success : Theme.warning
            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.md
                StatusBadge {
                    label: audio ? audio.virtualOutputStatus : qsTr("Checking…")
                    kind: audio && audio.virtualOutputSelected ? "active"
                          : (audio && audio.virtualOutputAvailable ? "ready" : "warning")
                }
                Item { Layout.fillWidth: true }
                SignalButton {
                    text: qsTr("REFRESH")
                    compact: true
                    onClicked: if (audio) audio.refreshVirtualAudio()
                }
                SignalButton {
                    text: (audio && (audio.virtualOutputHeld || audio.virtualOutputSelected))
                          ? qsTr("RELEASE SYSTEM OUTPUT")
                          : qsTr("USE AS SYSTEM OUTPUT")
                    visible: root.linuxAudio && audio && audio.virtualOutputAvailable
                    compact: true
                    primary: audio && !audio.virtualOutputHeld && !audio.virtualOutputSelected
                    onClicked: {
                        if (!audio)
                            return
                        if (audio.virtualOutputHeld || audio.virtualOutputSelected)
                            audio.releaseVirtualOutputAsSystemDefault()
                        else
                            audio.selectVirtualOutputAsSystemDefault()
                    }
                }
                SignalButton {
                    text: root.windowsAudio ? qsTr("WINDOWS SOUND") : qsTr("SOUND SETTINGS")
                    visible: root.windowsAudio || root.linuxAudio
                    compact: true
                    primary: root.windowsAudio && audio && audio.virtualOutputAvailable && !audio.virtualOutputSelected
                    onClicked: if (audio) audio.openWindowsSoundSettings()
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textMuted
                text: audio && audio.virtualOutputAvailable
                      ? (root.linuxAudio
                         ? qsTr("GNOME Settings can meter this sink without making it the default, and Bluetooth often steals the default back to a headset. Click USE AS SYSTEM OUTPUT and leave it held. Then deactivate the session if it is active and activate it again. The session source is Auralis Virtual Output. Dual Bluetooth can still sound slightly out of sync. Routing is active only while Auralis is running.")
                         : qsTr("Choose Auralis Virtual Output as the Windows output, then activate the session. Auralis captures that endpoint's Windows mix and fans it out only to the devices in your active session."))
                      : (root.linuxAudio
                         ? qsTr("PipeWire could not publish the virtual output. Check Diagnostics and ensure PipeWire and WirePlumber are running in this user session.")
                         : qsTr("The current application-capture fallback observes a copy after Windows has already sent audio to its selected device. A virtual endpoint removes that duplicate physical path, but Windows will only load an installed and trusted audio driver."))
            }
        }

        SectionCard {
            title: qsTr("Sources")
            subtitle: qsTr("Auralis Virtual Output is the only session source")
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
                message: qsTr("Waiting for Auralis Virtual Output.")
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

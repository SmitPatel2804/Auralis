import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "routePlanner"

    property var audio: null
    property bool showDeveloperDetail: false

    readonly property var router: audio ? audio.router : null
    readonly property bool sourceSelected: router
        && sourceCombo.currentValue !== undefined
        && sourceCombo.currentValue !== ""
    readonly property int selectedDestinationCount: selectedDestinations().length
    readonly property bool canActivate: sourceSelected && selectedDestinationCount > 0

    spacing: 8

    function selectedDestinations() {
        var dests = []
        for (var i = 0; i < destRepeater.count; ++i) {
            var item = destRepeater.itemAt(i)
            if (item && item.checked)
                dests.push(item.endpointId)
        }
        return dests
    }

    function activateClicked() {
        if (!router || sourceCombo.currentValue === undefined || sourceCombo.currentValue === "")
            return
        var dests = selectedDestinations()
        if (dests.length === 0)
            return
        var id = router.currentRouteId
        if (!id || id.length === 0)
            id = router.createRoute(sourceCombo.currentValue, dests)
        else {
            router.setRouteSource(id, sourceCombo.currentValue)
            router.setRouteDestinations(id, dests)
        }
        if (id && id.length > 0)
            router.activateRoute(id)
    }

    function deactivateClicked() {
        if (!router || !router.currentRouteId)
            return
        router.deactivateRoute(router.currentRouteId)
    }

    SignalComboBox {
        id: sourceCombo
        objectName: "routeSourceSelector"
        Layout.fillWidth: true
        model: router ? router.sources : null
        textRole: "name"
        valueRole: "sourceId"
        displayText: currentIndex >= 0 ? currentText : "Select a source"
        enabled: router && router.sourceCount > 0
        delegate: ItemDelegate {
            required property string name
            required property string sourceType
            required property string applicationName
            required property bool monitorSource
            required property bool available
            width: sourceCombo.width
            contentItem: SourceRow {
                name: name
                sourceType: sourceType
                applicationName: applicationName
                monitorSource: monitorSource
                available: available
            }
        }
    }

    Text {
        visible: !router || router.sourceCount === 0
        text: "No routable sources yet. Play audio or wait for a capture source."
        font.pixelSize: 12
        color: Theme.textMuted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    Text {
        text: "Playback destinations"
        font.pixelSize: 13
        font.bold: true
        color: Theme.text
    }

    Flickable {
        Layout.fillWidth: true
        Layout.preferredHeight: 120
        clip: true
        contentHeight: destColumn.implicitHeight

        Column {
            id: destColumn
            width: parent.width
            spacing: 2

            Repeater {
                id: destRepeater
                model: audio ? audio.endpoints : null
                delegate: CheckBox {
                    objectName: "routeDestinationOption"
                    required property string endpointId
                    required property string name
                    required property string direction
                    required property bool available
                    width: destColumn.width
                    visible: direction === "Playback" || direction === "Duplex"
                    height: visible ? implicitHeight : 0
                    text: name + (available ? "" : " (unavailable)")
                    enabled: available
                }
            }
        }
    }

    RowLayout {
        spacing: 12

        SignalButton {
            objectName: "routePlannerActivate"
            text: "Activate"
            primary: true
            enabled: root.canActivate
            onClicked: root.activateClicked()
        }

        SignalButton {
            objectName: "routePlannerDeactivate"
            text: "Deactivate"
            enabled: router && router.routeEnabled
            onClicked: root.deactivateClicked()
        }
    }

    Text {
        visible: root.sourceSelected && root.selectedDestinationCount === 0
        text: "Select at least one available playback destination to activate a route."
        font.pixelSize: 12
        color: Theme.textMuted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    Text {
        text: "State: " + (router ? router.routeStateText : "Inactive")
        font.pixelSize: 13
        color: Theme.text
    }

    Text {
        visible: router && router.lastErrorText.length > 0
        text: router ? router.lastErrorText : ""
        font.pixelSize: 12
        color: Theme.danger
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    RowLayout {
        visible: router && router.volumeCapable
        spacing: 12
        Layout.fillWidth: true

        Text {
            text: "Route volume"
            font.pixelSize: 13
            color: Theme.text
        }

        Slider {
            Layout.fillWidth: true
            from: 0
            to: 1
            value: router ? router.routeVolume : 1
            onMoved: {
                if (router && router.currentRouteId)
                    router.setRouteVolume(router.currentRouteId, value)
            }
        }

        CheckBox {
            text: "Mute"
            checked: router ? router.routeMuted : false
            onToggled: {
                if (router && router.currentRouteId)
                    router.setRouteMuted(router.currentRouteId, checked)
            }
        }
    }

    Text {
        visible: router && !router.volumeCapable && router.currentRouteId.length > 0
        text: "Volume control is not available for the current destinations."
        font.pixelSize: 12
        color: Theme.textMuted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    Text {
        visible: root.showDeveloperDetail && router
        text: router
              ? ("owned links=" + router.ownedLinkCount + " · route=" + router.currentRouteId)
              : ""
        font.pixelSize: 11
        color: Theme.textMuted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}

import QtQuick
import Auralis 1.0
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    width: parent ? parent.width : implicitWidth
    spacing: Metrics.xs

    Repeater {
        model: [
            { title: qsTr("Dashboard"), page: 0 },
            { title: qsTr("Devices"), page: 1 },
            { title: qsTr("Sessions"), page: 2 },
            { title: qsTr("Audio Routing"), page: 3 },
            { title: qsTr("Diagnostics"), page: 4 },
            { title: qsTr("Settings"), page: 5 }
        ]
        delegate: Button {
            required property string title
            required property int page
            Layout.fillWidth: true
            text: title
            checkable: true
            checked: AppCore.currentPage === page
            Accessible.name: title
            ToolTip.visible: hovered
            ToolTip.text: title
            onClicked: AppCore.navigateTo(page)
        }
    }
}

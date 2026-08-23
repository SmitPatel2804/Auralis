import QtQuick
import Auralis 1.0
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    width: parent ? parent.width : implicitWidth
    spacing: 6

    Label {
        text: qsTr("NAVIGATION")
        color: Theme.textFaint
        font.pixelSize: 9
        font.bold: true
        font.letterSpacing: 1.45
        Layout.leftMargin: Metrics.xs
        Layout.bottomMargin: Metrics.xs
    }

    Repeater {
        model: [
            { glyph: "01", title: qsTr("Dashboard"), page: 0 },
            { glyph: "02", title: qsTr("Devices"), page: 1 },
            { glyph: "03", title: qsTr("Sessions"), page: 2 },
            { glyph: "04", title: qsTr("Playground"), page: 3 },
            { glyph: "05", title: qsTr("Audio Routing"), page: 4 },
            { glyph: "06", title: qsTr("Diagnostics"), page: 5 },
            { glyph: "07", title: qsTr("Settings"), page: 6 }
        ]

        delegate: Button {
            id: navButton
            objectName: page === 0 ? "primaryNavigationTarget" : "navigationTarget"
            required property string glyph
            required property string title
            required property int page

            Layout.fillWidth: true
            implicitHeight: 46
            text: title
            checkable: true
            checked: AppCore.currentPage === page
            hoverEnabled: true
            leftPadding: Metrics.sm
            rightPadding: Metrics.sm
            Accessible.name: title
            ToolTip.visible: hovered
            ToolTip.text: title
            onClicked: AppCore.navigateTo(page)

            contentItem: RowLayout {
                spacing: Metrics.sm
                Label {
                    text: navButton.glyph
                    color: navButton.checked ? Theme.accent : Theme.textFaint
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 0.8
                    Layout.preferredWidth: 22
                }
                Label {
                    text: navButton.title
                    color: navButton.checked ? Theme.text : Theme.textMuted
                    font.pixelSize: 13
                    font.weight: navButton.checked ? Font.DemiBold : Font.Normal
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Rectangle {
                    visible: navButton.checked
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 6
                    radius: 3
                    color: Theme.accent
                }
            }

            background: Rectangle {
                radius: 12
                border.width: navButton.visualFocus ? 2 : (navButton.checked ? 1 : 0)
                border.color: navButton.visualFocus
                              ? Theme.focusRing
                              : Theme.alpha(Theme.accent, 0.36)
                color: navButton.checked
                       ? Theme.alpha(Theme.accent, 0.11)
                       : (navButton.hovered ? Theme.alpha(Theme.surfaceHover, 0.68) : "transparent")

                Rectangle {
                    visible: navButton.checked
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 22
                    radius: 2
                    color: Theme.accent
                }

                Behavior on color {
                    ColorAnimation { duration: Theme.duration }
                }
            }
        }
    }
}

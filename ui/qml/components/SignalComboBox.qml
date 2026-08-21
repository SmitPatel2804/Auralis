import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    implicitHeight: 42
    leftPadding: 14
    rightPadding: 42
    hoverEnabled: true

    delegate: ItemDelegate {
        width: control.width
        height: 40
        highlighted: control.highlightedIndex === index
        text: control.textAt(index)
        font.pixelSize: 12
        contentItem: Label {
            text: parent.text
            color: parent.highlighted ? Theme.text : Theme.textMuted
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            leftPadding: 8
        }
        background: Rectangle {
            radius: 9
            color: parent.highlighted ? Theme.alpha(Theme.accent, 0.16) : "transparent"
            border.color: parent.highlighted ? Theme.alpha(Theme.accent, 0.42) : "transparent"
        }
    }

    indicator: Item {
        x: control.width - width - 12
        y: (control.height - height) / 2
        width: 22
        height: 22
        Rectangle {
            anchors.fill: parent
            radius: 7
            color: Theme.alpha(control.down ? Theme.accent : Theme.surfaceRaised, 0.78)
            border.color: Theme.alpha(control.activeFocus ? Theme.accent : Theme.borderBright, 0.48)
        }
        Label {
            anchors.centerIn: parent
            text: control.popup.visible ? "⌃" : "⌄"
            color: control.activeFocus || control.hovered ? Theme.accent : Theme.textMuted
            font.pixelSize: 13
            font.bold: true
        }
    }

    contentItem: Label {
        leftPadding: 2
        rightPadding: 4
        text: control.displayText
        color: control.enabled ? Theme.text : Theme.textFaint
        font.pixelSize: 12
        font.weight: Font.Medium
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 12
        color: Theme.alpha(control.down ? Theme.surfaceHover : Theme.surfaceRaised, 0.9)
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus
                      ? Theme.accent
                      : Theme.alpha(control.hovered ? Theme.borderBright : Theme.border, control.hovered ? 0.78 : 0.62)
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            height: 1
            color: Theme.alpha(Theme.accent, control.activeFocus ? 0.7 : 0.18)
        }
    }

    popup: Popup {
        y: control.height + 6
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 360)
        padding: 6
        background: Rectangle {
            radius: 13
            color: Theme.backgroundElevated
            border.width: 1
            border.color: Theme.alpha(Theme.accent, 0.44)
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
}

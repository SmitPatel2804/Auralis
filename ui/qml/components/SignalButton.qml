import QtQuick
import QtQuick.Controls

Button {
    id: root

    property bool primary: false
    property bool danger: false
    property bool compact: false

    implicitHeight: compact ? 34 : Theme.controlHeight
    implicitWidth: Math.max(90, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: compact ? Metrics.sm : Metrics.md
    rightPadding: compact ? Metrics.sm : Metrics.md
    hoverEnabled: true
    activeFocusOnTab: true
    Accessible.name: text

    contentItem: Label {
        text: root.text
        color: !root.enabled
               ? Theme.textFaint
               : (root.primary ? Theme.background : (root.danger ? Theme.danger : Theme.text))
        font.pixelSize: root.compact ? 11 : 12
        font.bold: true
        font.letterSpacing: 0.45
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 11
        border.width: root.visualFocus ? 2 : 1
        border.color: !root.enabled
                      ? Theme.border
                      : (root.visualFocus
                         ? Theme.focusRing
                      : (root.danger
                         ? Theme.alpha(Theme.danger, 0.58)
                         : Theme.alpha(root.primary ? Theme.accent : Theme.borderBright, 0.58)))
        color: {
            if (!root.enabled)
                return Theme.alpha(Theme.surfaceRaised, 0.45)
            if (root.down)
                return root.primary ? Theme.info : Theme.surface
            if (root.primary)
                return root.hovered ? Theme.accentHover : Theme.accent
            if (root.danger)
                return Theme.alpha(Theme.danger, root.hovered ? 0.18 : 0.1)
            return root.hovered ? Theme.surfaceHover : Theme.surfaceRaised
        }

        Behavior on color { ColorAnimation { duration: Theme.duration } }
        Behavior on border.color { ColorAnimation { duration: Theme.duration } }
    }
}

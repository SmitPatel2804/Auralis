import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

ApplicationWindow {
    id: root
    visible: true
    width: AppCore.configuration ? AppCore.configuration.windowWidth : 1280
    height: AppCore.configuration ? AppCore.configuration.windowHeight : 800
    minimumWidth: 880
    minimumHeight: 600
    title: "Auralis"
    color: Theme.background

    palette.window: Theme.background
    palette.windowText: Theme.text
    palette.base: Theme.surfaceAlt
    palette.alternateBase: Theme.surfaceRaised
    palette.text: Theme.text
    palette.button: Theme.surfaceRaised
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.background
    palette.placeholderText: Theme.textFaint
    palette.toolTipBase: Theme.surfaceRaised
    palette.toolTipText: Theme.text

    onWidthChanged: if (AppCore.configuration) AppCore.configuration.setWindowWidth(width)
    onHeightChanged: if (AppCore.configuration) AppCore.configuration.setWindowHeight(height)

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#071321" }
            GradientStop { position: 0.48; color: Theme.background }
            GradientStop { position: 1.0; color: "#080816" }
        }
    }

    // Quiet technical grid: enough depth to feel spatial, faint enough that
    // data remains the focal point.
    Item {
        anchors.fill: parent
        opacity: 0.32
        Repeater {
            model: Math.ceil(root.width / 56)
            Rectangle {
                required property int index
                x: index * 56
                width: 1
                height: root.height
                color: Theme.alpha(Theme.accent, 0.08)
            }
        }
        Repeater {
            model: Math.ceil(root.height / 56)
            Rectangle {
                required property int index
                y: index * 56
                width: root.width
                height: 1
                color: Theme.alpha(Theme.accent, 0.06)
            }
        }
    }

    Rectangle {
        width: 460
        height: 460
        radius: width / 2
        x: root.width - width * 0.55
        y: -height * 0.52
        color: Theme.alpha(Theme.accent, 0.035)
        border.color: Theme.alpha(Theme.accent, 0.12)
    }

    Rectangle {
        width: 380
        height: 380
        radius: width / 2
        x: -width * 0.52
        y: root.height - height * 0.48
        color: Theme.alpha(Theme.accentSecondary, 0.03)
        border.color: Theme.alpha(Theme.accentSecondary, 0.10)
    }

    AppShell {
        anchors.fill: parent
        anchors.margins: Metrics.sm
    }
}

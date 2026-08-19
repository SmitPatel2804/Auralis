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

    onWidthChanged: if (AppCore.configuration) AppCore.configuration.setWindowWidth(width)
    onHeightChanged: if (AppCore.configuration) AppCore.configuration.setWindowHeight(height)

    AppShell {
        anchors.fill: parent
        anchors.margins: Metrics.md
    }
}

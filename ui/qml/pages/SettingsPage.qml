import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Auralis 1.0

Flickable {
    id: root
    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight
    readonly property var config: AppCore.configuration

    ColumnLayout {
        id: column
        width: root.width
        spacing: Metrics.md

        PageHeader { title: qsTr("Control Parameters"); subtitle: qsTr("Persistent interface, recovery and telemetry preferences") }

        ErrorBanner { text: config ? config.lastErrorText : "" }

        SectionCard {
            title: qsTr("Interface")
            subtitle: qsTr("Startup and resilience behavior")
            signalColor: Theme.accent
            CheckBox {
                text: qsTr("Show developer diagnostics")
                checked: config ? config.showDeveloperStatus : false
                enabled: config && !config.developerStatusEnvLocked
                onToggled: config.setShowDeveloperStatus(checked)
            }
            CheckBox {
                text: qsTr("Restore last session on startup")
                checked: config ? config.restoreLastSession : false
                onToggled: config.setRestoreLastSession(checked)
            }
            CheckBox {
                text: qsTr("Auto-recover Bluetooth and audio services")
                checked: config ? config.autoRecoverServices : true
                onToggled: config.setAutoRecoverServices(checked)
            }
            CheckBox {
                text: qsTr("Restore services after suspend/resume")
                checked: config ? config.restoreOnResume : true
                onToggled: config.setRestoreOnResume(checked)
            }
            Label {
                visible: config && config.developerStatusEnvLocked
                text: qsTr("Developer status is locked by AURALIS_UI_SHOW_DEVELOPER_STATUS (restart not required for other settings).")
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        SectionCard {
            title: qsTr("Logging")
            subtitle: qsTr("A timestamped execution log is enabled by default")
            signalColor: Theme.accentSecondary
            CheckBox {
                text: qsTr("Enable file logging")
                checked: config ? config.fileLoggingEnabled : false
                enabled: config && !config.fileLoggingEnvLocked
                onToggled: {
                    if (!config.setFileLoggingEnabled(checked))
                        checked = Qt.binding(function() { return config ? config.fileLoggingEnabled : false })
                }
            }
            Label {
                text: qsTr("A new timestamped file is created for each default-configured run. Audio samples and secrets are never written to logs. Changing this path applies on the next restart.")
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Log file path")
                color: Theme.text
            }
            TextField {
                Layout.fillWidth: true
                text: config ? config.logFilePath : ""
                enabled: config && !config.fileLoggingEnvLocked
                placeholderText: qsTr("Log file path")
                Accessible.name: qsTr("Log file path")
                onEditingFinished: config.setLogFilePath(text)
                leftPadding: Metrics.md
                rightPadding: Metrics.md
                background: Rectangle {
                    radius: 11
                    color: Theme.surfaceRaised
                    border.color: parent.activeFocus ? Theme.accent : Theme.border
                }
            }
        }

        SignalButton {
            objectName: "settingsResetButton"
            text: qsTr("RESET TO DEFAULTS")
            danger: true
            onClicked: resetDialog.open()
        }
    }

    ConfirmDialog {
        id: resetDialog
        title: qsTr("Reset settings")
        message: qsTr("Reset UI and logging preferences to defaults?")
        confirmText: qsTr("Reset")
        onConfirmed: config.resetToDefaults()
    }
}

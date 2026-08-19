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

        PageHeader { title: qsTr("Settings"); subtitle: qsTr("Persisted through ConfigurationManager") }

        ErrorBanner { text: config ? config.lastErrorText : "" }

        SectionCard {
            title: qsTr("Interface")
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
            CheckBox {
                text: qsTr("Enable file logging")
                checked: config ? config.fileLoggingEnabled : false
                enabled: config && !config.fileLoggingEnvLocked
                onToggled: config.setFileLoggingEnabled(checked)
            }
            Label {
                text: qsTr("Changing the log path requires an application restart to take effect.")
                color: Theme.textMuted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            TextField {
                Layout.fillWidth: true
                text: config ? config.logFilePath : ""
                enabled: config && !config.fileLoggingEnvLocked
                placeholderText: qsTr("Log file path")
                onEditingFinished: config.setLogFilePath(text)
            }
        }

        Button {
            text: qsTr("Reset to defaults")
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

pragma Singleton
import QtQuick

QtObject {
    // Deep-space neutrals keep dense routing information readable while the
    // cyan/violet signal colors make live state feel immediate.
    readonly property color background: "#050912"
    readonly property color backgroundElevated: "#08111D"
    readonly property color surface: "#0B1624"
    readonly property color surfaceAlt: "#0E1D2E"
    readonly property color surfaceRaised: "#13263A"
    readonly property color surfaceHover: "#173149"
    readonly property color border: "#1B354B"
    readonly property color borderBright: "#2B6680"
    readonly property color text: "#F2F8FF"
    readonly property color textMuted: "#8AA2B8"
    readonly property color textFaint: "#587086"
    readonly property color accent: "#21D4FD"
    readonly property color accentSecondary: "#8B5CF6"
    readonly property color accentSoft: "#164E63"
    readonly property color success: "#2DD4BF"
    readonly property color warning: "#FBBF24"
    readonly property color danger: "#FB7185"
    readonly property color info: "#38BDF8"
    readonly property color focusRing: "#67E8F9"
    readonly property real disabledOpacity: 0.45
    readonly property int radius: 12
    readonly property int cardRadius: 16
    readonly property int controlHeight: 40
    readonly property int duration: 180

    function alpha(color, amount) {
        return Qt.rgba(color.r, color.g, color.b, amount)
    }

    function statusColor(kind) {
        switch (kind) {
        case "ready":
        case "active":
        case "connected":
            return success
        case "busy":
        case "scanning":
        case "recovering":
            return info
        case "warning":
        case "degraded":
            return warning
        case "error":
        case "failed":
            return danger
        default:
            return textMuted
        }
    }
}

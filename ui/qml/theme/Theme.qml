pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#0f1419"
    readonly property color surface: "#1a222c"
    readonly property color surfaceAlt: "#232d3a"
    readonly property color border: "#334155"
    readonly property color text: "#e8eef5"
    readonly property color textMuted: "#94a3b8"
    readonly property color accent: "#3b82f6"
    readonly property color success: "#22c55e"
    readonly property color warning: "#f59e0b"
    readonly property color danger: "#ef4444"
    readonly property color info: "#38bdf8"
    readonly property color focusRing: "#93c5fd"
    readonly property real disabledOpacity: 0.45
    readonly property int radius: 10
    readonly property int controlHeight: 36
    readonly property int duration: 120

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

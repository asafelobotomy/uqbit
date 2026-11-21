pragma Singleton
import QtQuick
import Uqbit

// Every colour, radius and font the window uses. On uQ they come from uQ's theme tokens and
// follow theme changes live; elsewhere uQbit uses a neutral dark palette (uQ's Graphite).
QtObject {
    readonly property var tokens: App.theme.tokens
    readonly property bool uq: tokens.present === true

    function pick(key, fallback) {
        return uq && tokens.colors && tokens.colors[key] ? tokens.colors[key] : fallback;
    }

    readonly property color bg: pick("bg", "#1f1f22")
    readonly property color surface: pick("surface", "#2b2b2e")
    readonly property color surfaceAlt: pick("surface_alt", "#353538")
    readonly property color overlay: pick("overlay", "#464649")
    readonly property color text: pick("text", "#e4e4e7")
    readonly property color textMuted: pick("text_muted", "#a9a9ac")
    readonly property color accent: pick("accent", "#8ab4f8")
    readonly property color accentText: pick("accent_text", "#1f1f22")
    readonly property color border: pick("border", "#5a5a5d")
    readonly property color success: pick("success", "#81c995")
    readonly property color warning: pick("warning", "#fdd663")
    readonly property color error: pick("error", "#f28b82")

    readonly property color scrim: "#000000"

    readonly property int radius: uq && tokens.radius !== undefined ? tokens.radius : 10
    readonly property string fontUi: uq && tokens.fontUi ? tokens.fontUi : Qt.application.font.family
    readonly property string fontMono: uq && tokens.fontMono ? tokens.fontMono : "monospace"
    readonly property real bodySize: uq && tokens.fontSize ? tokens.fontSize : 11
    readonly property real smallSize: Math.max(8, bodySize - 1.5)
    readonly property real headingSize: Math.round(bodySize * 1.6)
    readonly property int gap: 8
}

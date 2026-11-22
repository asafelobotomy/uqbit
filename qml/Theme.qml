pragma Singleton
import QtQuick
import Uqbit

// Every colour, radius, font size and speed the window uses: uQ's shared theme base
// (UqThemeBase.qml, vendored unchanged from uQ's native/uq-consts). On uQ it follows uQ's theme
// and Reduce motion live; elsewhere it is uQ's Graphite palette.
UqThemeBase {
    tokens: App.theme.tokens

    readonly property color scrim: "#000000"
}

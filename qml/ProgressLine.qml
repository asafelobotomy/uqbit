import QtQuick
import Uqbit

// A thin rounded progress bar in one of the theme's colours.
Rectangle {
    id: line
    property real value: 0
    property color tint: Theme.accent
    implicitHeight: 6
    radius: height / 2
    color: Qt.alpha(Theme.text, 0.1)
    Rectangle {
        width: Math.max(line.value > 0 ? line.height : 0, line.width * Math.max(0, Math.min(1, line.value)))
        height: parent.height
        radius: parent.radius
        color: line.tint
        Behavior on width { NumberAnimation { duration: Theme.motionSlow; easing.type: Easing.OutCubic } }
    }
}

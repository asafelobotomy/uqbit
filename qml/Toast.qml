import QtQuick
import QtQuick.Controls
import Uqbit

// A short message at the bottom ("Added to ~/Downloads") that fades by itself.
Rectangle {
    id: toast
    function show(text) {
        label.text = text;
        toast.opacity = 1;
        hide.restart();
    }

    width: Math.min(label.implicitWidth + 32, parent ? parent.width - 40 : 600)
    height: label.implicitHeight + 18
    radius: Math.min(Theme.radius + 4, height / 2)
    color: Theme.overlay
    border.color: Qt.alpha(Theme.border, 0.6)
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.motionSlow; easing.type: Theme.easing } }

    Label {
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, toast.width - 32)
        elide: Text.ElideMiddle
        color: Theme.text
        font.family: Theme.fontUi
        font.pointSize: Theme.bodySize
    }
    Timer {
        id: hide
        interval: 2600
        onTriggered: toast.opacity = 0
    }
}

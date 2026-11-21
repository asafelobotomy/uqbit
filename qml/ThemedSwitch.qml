import QtQuick
import QtQuick.Controls
import Uqbit

// A switch in the theme's colours: the accent when on.
Switch {
    id: sw
    font.family: Theme.fontUi
    font.pointSize: Theme.bodySize
    spacing: 10
    indicator: Rectangle {
        x: sw.leftPadding
        y: (sw.height - height) / 2
        implicitWidth: 40
        implicitHeight: 22
        radius: height / 2
        color: sw.checked ? Theme.accent : Theme.surfaceAlt
        border.color: sw.checked ? Theme.accent : (sw.visualFocus ? Theme.accent : Theme.border)
        Rectangle {
            x: sw.checked ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            radius: 8
            color: sw.checked ? Theme.accentText : Theme.textMuted
            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }
    contentItem: Label {
        leftPadding: sw.indicator.width + sw.spacing
        verticalAlignment: Text.AlignVCenter
        text: sw.text
        color: Theme.text
        font: sw.font
    }
}

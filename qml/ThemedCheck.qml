import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import Uqbit

// A check box in the theme's colours: the accent with a tick when checked.
CheckBox {
    id: box
    font.family: Theme.fontUi
    font.pointSize: Theme.bodySize
    padding: 0
    spacing: 10
    indicator: Rectangle {
        x: box.leftPadding
        y: (box.height - height) / 2
        implicitWidth: 18
        implicitHeight: 18
        radius: Math.min(5, Theme.radius / 2)
        color: box.checked ? Theme.accent : "transparent"
        border.color: box.checked || box.visualFocus ? Theme.accent : Theme.border
        border.width: 1.5
        Impl.IconImage {
            anchors.centerIn: parent
            visible: box.checked
            source: "qrc:/qt/qml/Uqbit/icons/check.svg"
            sourceSize: Qt.size(14, 14)
            color: Theme.accentText
        }
    }
    contentItem: Label {
        leftPadding: box.text !== "" ? box.indicator.width + box.spacing : box.indicator.width
        verticalAlignment: Text.AlignVCenter
        text: box.text
        color: Theme.text
        font: box.font
    }
}

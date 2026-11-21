import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl

// A text button in the theme: filled with the accent when `primary`, else a quiet surface.
AbstractButton {
    id: btn
    property bool primary: false
    property string iconName

    implicitHeight: 36
    implicitWidth: row.implicitWidth + 28
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.45
    Accessible.name: text

    readonly property color fg: primary ? Theme.accentText : Theme.text

    background: Rectangle {
        radius: Theme.radius
        color: btn.primary ? Theme.accent : Theme.surfaceAlt
        border.color: btn.visualFocus ? Theme.accent : (btn.primary ? "transparent" : Qt.alpha(Theme.border, 0.7))
        border.width: 1
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: btn.fg
            opacity: btn.down ? 0.18 : btn.hovered ? 0.08 : 0
        }
    }
    contentItem: Item {
        implicitWidth: row.implicitWidth
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8
            Impl.IconImage {
                visible: btn.iconName !== ""
                anchors.verticalCenter: parent.verticalCenter
                source: btn.iconName ? "qrc:/qt/qml/Uqbit/icons/" + btn.iconName + ".svg" : ""
                sourceSize: Qt.size(18, 18)
                color: btn.fg
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: btn.text
                color: btn.fg
                font.family: Theme.fontUi
                font.pointSize: Theme.bodySize
                font.weight: btn.primary ? Font.DemiBold : Font.Normal
            }
        }
    }
}

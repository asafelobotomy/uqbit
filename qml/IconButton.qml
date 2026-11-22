import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl

// A round, flat button with one of uQbit's icons, tinted to `tint`, and a tooltip.
AbstractButton {
    id: btn
    property string iconName
    property string tip
    property color tint: Theme.text
    property real size: 40
    property bool active: false

    implicitWidth: size
    implicitHeight: size
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    Accessible.name: tip
    display: AbstractButton.IconOnly
    icon.source: iconName ? "qrc:/qt/qml/Uqbit/icons/" + iconName + ".svg" : ""
    icon.width: Math.round(size * 0.55)
    icon.height: Math.round(size * 0.55)
    icon.color: btn.active ? Theme.accent : btn.tint

    background: Rectangle {
        radius: width / 2
        color: btn.tint
        opacity: btn.down ? 0.24 : btn.hovered ? 0.14 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motionBase; easing.type: Theme.easing } }
    }
    contentItem: Impl.IconImage {
        source: btn.icon.source
        sourceSize: Qt.size(btn.icon.width, btn.icon.height)
        color: btn.icon.color
        opacity: btn.enabled ? 1 : 0.4
        fillMode: Image.Pad
    }

    ToolTip.visible: hovered && tip.length > 0
    ToolTip.delay: 600
    ToolTip.text: tip
}

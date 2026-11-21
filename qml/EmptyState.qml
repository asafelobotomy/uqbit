import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import Uqbit

// The middle of the window when there's nothing to list, or the engine isn't there.
ColumnLayout {
    id: empty
    property string icon: "uqbit"
    property string title
    property string detail
    property bool busy: false
    property bool tinted: true
    default property alias actions: actionRow.data

    spacing: 10
    BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        visible: empty.busy
        running: empty.busy
    }
    Image {
        Layout.alignment: Qt.AlignHCenter
        visible: !empty.busy && empty.icon === "uqbit"
        source: "qrc:/qt/qml/Uqbit/icons/uqbit.svg"
        sourceSize: Qt.size(84, 84)
        opacity: 0.9
    }
    Impl.IconImage {
        Layout.alignment: Qt.AlignHCenter
        visible: !empty.busy && empty.icon !== "uqbit"
        source: empty.icon !== "uqbit" ? "qrc:/qt/qml/Uqbit/icons/" + empty.icon + ".svg" : ""
        sourceSize: Qt.size(44, 44)
        color: Theme.textMuted
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        text: empty.title
        color: Theme.text
        font.family: Theme.fontUi
        font.pointSize: Theme.bodySize + 3
        font.weight: Font.DemiBold
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 420
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: empty.detail
        color: Theme.textMuted
        font.family: Theme.fontUi
        font.pointSize: Theme.bodySize
    }
    RowLayout {
        id: actionRow
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 6
        spacing: 8
    }
}

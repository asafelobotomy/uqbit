import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Uqbit

// "Remove <name>?", with or without its downloaded files.
Popup {
    id: dialog
    property string hash
    property string name
    function ask(h, n) {
        hash = h;
        name = n;
        withFiles.checked = false;
        open();
    }

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(480, parent ? parent.width - 40 : 480)
    padding: 20
    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius + 2
        border.color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Label {
            Layout.fillWidth: true
            text: qsTr("Remove “%1”?").arg(dialog.name)
            wrapMode: Text.Wrap
            color: Theme.text
            font.family: Theme.fontUi
            font.pointSize: Theme.bodySize + 2
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: withFiles.checked ? qsTr("The torrent and everything it downloaded go. This can't be undone.")
                                    : qsTr("uQbit stops sharing it. What it downloaded stays where it is.")
            color: withFiles.checked ? Theme.error : Theme.textMuted
            font.family: Theme.fontUi
            font.pointSize: Theme.smallSize
        }
        ThemedCheck {
            id: withFiles
            text: qsTr("Also delete its files")
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            PillButton {
                text: qsTr("Cancel")
                onClicked: dialog.close()
            }
            PillButton {
                primary: true
                text: withFiles.checked ? qsTr("Remove and delete") : qsTr("Remove")
                onClicked: {
                    App.remove([dialog.hash], withFiles.checked);
                    dialog.close();
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Uqbit
import "logic.js" as Logic

// Paste one or more magnet links (or links to .torrent files), one per line.
Popup {
    id: dialog
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, parent ? parent.width - 40 : 560)
    padding: 20
    onOpened: { field.text = ""; field.forceActiveFocus(); }

    readonly property var links: Logic.torrentInputs([field.text])

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius + 2
        border.color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Label {
            text: qsTr("Add magnet links")
            color: Theme.text
            font.family: Theme.fontUi
            font.pointSize: Theme.bodySize + 3
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Paste magnet: links or links to .torrent files, one per line.")
            color: Theme.textMuted
            font.family: Theme.fontUi
            font.pointSize: Theme.smallSize
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 110
            TextArea {
                id: field
                placeholderText: "magnet:?xt=urn:btih:…"
                wrapMode: TextEdit.WrapAnywhere
                color: Theme.text
                font.family: Theme.fontMono
                font.pointSize: Theme.smallSize
                selectByMouse: true
                background: Rectangle {
                    color: Theme.bg
                    radius: Theme.radius
                    border.color: field.activeFocus ? Theme.accent : Theme.border
                }
            }
        }
        RowLayout {
            Label {
                Layout.fillWidth: true
                text: field.text.trim() === "" ? "" : dialog.links.length === 0 ? qsTr("No magnet links in that")
                      : dialog.links.length === 1 ? qsTr("1 link") : qsTr("%1 links").arg(dialog.links.length)
                color: dialog.links.length === 0 ? Theme.warning : Theme.textMuted
                font.family: Theme.fontUi
                font.pointSize: Theme.smallSize
            }
            PillButton {
                text: qsTr("Cancel")
                onClicked: dialog.close()
            }
            PillButton {
                primary: true
                text: qsTr("Next")
                enabled: dialog.links.length > 0
                onClicked: {
                    const l = dialog.links;
                    dialog.close();
                    App.openInputs(l);
                }
            }
        }
    }
}

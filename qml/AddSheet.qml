import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import Uqbit
import "logic.js" as Logic

// What's about to be added: each torrent's name and size, where it saves, and whether it
// starts now. Opens by itself when links or files arrive (unless "ask before adding" is off).
Popup {
    id: sheet
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, parent ? parent.width - 40 : 560)
    padding: 20
    closePolicy: Popup.CloseOnEscape
    onClosed: App.clearPending()
    onOpened: {
        folder.text = App.prettyFolder(App.downloadFolder());
        start.checked = App.settings.start_added !== false;
        addButton.forceActiveFocus();
    }

    readonly property var items: App.pending
    readonly property int good: items.filter(i => i.ok).length

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius + 2
        border.color: Theme.border
    }

    FolderDialog {
        id: picker
        title: qsTr("Save the files in")
        onAccepted: folder.text = App.prettyFolder(selectedFolder.toString())
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            text: sheet.items.length === 1 ? qsTr("Add this torrent?") : qsTr("Add %1 torrents?").arg(sheet.items.length)
            color: Theme.text
            font.family: Theme.fontUi
            font.pointSize: Theme.bodySize + 3
            font.weight: Font.DemiBold
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 240)
            clip: true
            spacing: 6
            model: sheet.items
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width
                height: itemRow.implicitHeight + 16
                radius: Theme.radius
                color: Theme.surfaceAlt
                RowLayout {
                    id: itemRow
                    anchors.fill: parent
                    anchors.margins: 8
                    anchors.leftMargin: 12
                    spacing: 10
                    Impl.IconImage {
                        source: "qrc:/qt/qml/Uqbit/icons/" + (modelData.ok ? (modelData.kind === "file" ? "file" : "magnet") : "error") + ".svg"
                        sourceSize: Qt.size(20, 20)
                        color: modelData.ok ? Theme.accent : Theme.error
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: modelData.name || (modelData.kind === "magnet" ? qsTr("Magnet link") : modelData.input)
                            elide: Text.ElideMiddle
                            color: Theme.text
                            font.family: Theme.fontUi
                            font.pointSize: Theme.bodySize
                        }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: !modelData.ok ? modelData.error
                                  : modelData.kind === "file"
                                    ? (Logic.bytes(modelData.size) + " · " + (modelData.file_count === 1 ? qsTr("1 file") : qsTr("%1 files").arg(modelData.file_count)))
                                  : modelData.kind === "magnet" ? qsTr("Magnet link: its files show once peers send its details")
                                  : modelData.input
                            color: modelData.ok ? Theme.textMuted : Theme.error
                            font.family: Theme.fontUi
                            font.pointSize: Theme.smallSize
                        }
                    }
                    IconButton {
                        size: 30
                        iconName: "close"
                        tip: qsTr("Leave this one out")
                        onClicked: App.dropPending(index)
                    }
                }
            }
        }

        Label {
            text: qsTr("Save in")
            color: Theme.textMuted
            font.family: Theme.fontUi
            font.pointSize: Theme.smallSize
        }
        RowLayout {
            spacing: 8
            TextField {
                id: folder
                Layout.fillWidth: true
                color: Theme.text
                font.family: Theme.fontUi
                selectByMouse: true
                background: Rectangle {
                    implicitHeight: 36
                    color: Theme.bg
                    radius: Theme.radius
                    border.color: folder.activeFocus ? Theme.accent : Theme.border
                }
            }
            PillButton {
                iconName: "folder"
                text: qsTr("Choose…")
                onClicked: picker.open()
            }
        }

        RowLayout {
            Layout.topMargin: 4
            ThemedSwitch {
                id: start
                text: qsTr("Start downloading now")
            }
            Item { Layout.fillWidth: true }
            PillButton {
                text: qsTr("Cancel")
                onClicked: sheet.close()
            }
            PillButton {
                id: addButton
                primary: true
                text: qsTr("Add")
                enabled: sheet.good > 0 && folder.text.trim() !== ""
                onClicked: {
                    App.addPending(folder.text.trim(), start.checked);
                    sheet.close();
                }
                Keys.onReturnPressed: clicked()
            }
        }
    }
}

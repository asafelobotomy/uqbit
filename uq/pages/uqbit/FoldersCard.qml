// The Folders card of uQbit's uQ Settings page: where torrents save, where unfinished ones wait,
// a folder whose .torrent files are added by themselves, and how files land on disk.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.uq.settings
import "logic.js" as Logic

SettingsCard {
    id: card
    property var page
    title: "Folders"
    iconName: "folder-download"
    help: "The torrent engine may write only to these folders and the ones you added torrents to. A new folder restarts it once to let it."

    FolderRow {
        page: card.page
        key: "download_folder"
        label: "Save new torrents in"
    }
    FolderRow {
        page: card.page
        key: "incomplete_folder"
        label: "Keep unfinished downloads in"
        placeholder: "Off: in the save folder"
        optional: true
        tip: "Finished torrents move to their save folder, so it only ever holds complete files."
    }
    FolderRow {
        page: card.page
        key: "watch_folder"
        label: "Add .torrent files saved to"
        placeholder: "Off"
        optional: true
        tip: "uQbit adds a .torrent file saved to this folder by itself and deletes the file once it's added."
    }
    SettingRow {
        settingId: "content_layout"
        label: "Put each torrent's files"
        highlight: card.page.highlight
        UqComboBox {
            id: layout
            objectName: "content_layout-combo"
            readonly property var values: ["original", "subfolder", "no_subfolder"]
            model: ["As the torrent has them", "Always in a folder of their own", "Never in a folder"]
            currentIndex: Math.max(0, values.indexOf(card.page.value("content_layout") || "original"))
            enabled: card.page.ready
            onActivated: i => {
                if (values[i] !== card.page.value("content_layout"))
                    card.page.setKey("content_layout", values[i]);
            }
        }
    }
    SwitchRow {
        settingId: "incomplete_extension"
        label: "Mark unfinished files with .!qB"
        highlight: card.page.highlight
        value: card.page.value("incomplete_extension") === true
        enabled: card.page.ready
        onChosen: v => card.page.setKey("incomplete_extension", Logic.boolArg(v))
    }
    SwitchRow {
        settingId: "preallocate"
        label: "Reserve the disk space when a torrent is added"
        highlight: card.page.highlight
        value: card.page.value("preallocate") === true
        enabled: card.page.ready
        onChosen: v => card.page.setKey("preallocate", Logic.boolArg(v))
        HelpTip { tip: "Fails early when the disk is too full, and keeps the files in one piece on hard disks. Takes longer to add big torrents." }
    }
    UqLabel {
        visible: text !== ""
        text: card.page.notes["content_layout"] || card.page.notes["incomplete_extension"] || card.page.notes["preallocate"] || ""
        color: card.page.errorColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}

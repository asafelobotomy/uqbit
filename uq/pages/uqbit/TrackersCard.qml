// The Trackers card of uQbit's uQ Settings page: links to public tracker lists (any of up to
// 10) whose trackers are added to new public torrents, so they find more peers.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.uq.settings
import "logic.js" as Logic

SettingsCard {
    id: card
    property var page
    title: "Tracker lists"
    iconName: "network-server"
    help: "uQbit downloads these lists every day and adds their trackers to new public torrents. Private torrents keep only their own trackers."

    readonly property string value: card.page.value("tracker_lists") || ""
    readonly property var typed: Logic.addTrackerList(card.value, field.text)

    Repeater {
        model: Logic.trackerUrls(card.value)
        delegate: SettingRow {
            required property string modelData
            Layout.fillWidth: true
            settingId: "tracker_lists"
            label: modelData
            highlight: card.page.highlight
            QQC2.Button {
                text: "Remove"
                enabled: card.page.workingKey === ""
                onClicked: card.page.setKey("tracker_lists", Logic.removeTrackerList(card.value, modelData))
            }
        }
    }
    SettingRow {
        settingId: "tracker_lists"
        label: Logic.trackerUrls(card.value).length === 0 ? "Add a list" : "Add another list"
        highlight: card.page.highlight
        QQC2.TextField {
            id: field
            objectName: "tracker-list-field"
            placeholderText: "https://…/trackers_best.txt"
            Layout.preferredWidth: 260
            onAccepted: add.clicked()
        }
        QQC2.Button {
            id: add
            objectName: "tracker-list-add"
            text: "Add"
            enabled: card.page.workingKey === "" && card.typed !== null
            onClicked: {
                card.page.setKey("tracker_lists", card.typed);
                field.text = "";
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        UqLabel {
            text: Logic.trackersLine(card.page.status)
            color: card.page.mutedColor
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        QQC2.Button {
            text: "Update now"
            visible: Logic.trackerUrls(card.value).length > 0
            enabled: card.page.workingKey === ""
            onClicked: card.page.action("trackers-update", "tracker_lists")
        }
    }
    UqLabel {
        visible: text !== ""
        text: card.page.notes["tracker_lists"] || ""
        color: card.page.errorColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}

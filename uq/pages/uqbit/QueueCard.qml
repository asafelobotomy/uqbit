// The Queue and seeding card of uQbit's uQ Settings page: how many torrents run at once, and
// when a torrent stops seeding (and whether it then leaves the list).
import QtQuick
import QtQuick.Layouts
import org.uq.settings
import "logic.js" as Logic

SettingsCard {
    id: card
    property var page
    title: "Queue and seeding"
    iconName: "view-sort"
    help: "0 means no limit."

    NumberRow {
        page: card.page
        key: "max_active_downloads"
        label: "Download at most"
        max: 999
        after: "torrents at once"
    }
    NumberRow {
        page: card.page
        key: "max_active_uploads"
        label: "Seed at most"
        max: 999
        after: "torrents at once"
    }
    NumberRow {
        page: card.page
        key: "max_active_torrents"
        label: "Run at most"
        max: 999
        after: "torrents at once in all"
        tip: "Torrents past these limits wait in a queue and start when one before them finishes."
    }
    UqLabel {
        text: Logic.queueLine(card.page.config)
        color: card.page.mutedColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    NumberRow {
        page: card.page
        key: "seed_ratio"
        label: "Stop seeding at ratio"
        max: 100
        tip: "A ratio of 2 stops a torrent once it has shared twice its size."
    }
    NumberRow {
        page: card.page
        key: "seed_time"
        label: "Stop seeding after (minutes)"
        max: 525600
        after: Logic.minutesLabel(card.page.value("seed_time"))
    }
    SettingRow {
        settingId: "when_seeded"
        label: "When a torrent is done seeding"
        highlight: card.page.highlight
        UqComboBox {
            objectName: "when_seeded-combo"
            readonly property var values: ["stop", "remove"]
            model: ["Stop it", "Remove it from the list (keep the files)"]
            currentIndex: Math.max(0, values.indexOf(card.page.value("when_seeded") || "stop"))
            enabled: card.page.ready
            onActivated: i => {
                if (values[i] !== card.page.value("when_seeded"))
                    card.page.setKey("when_seeded", values[i]);
            }
        }
    }
    UqLabel {
        visible: text !== ""
        text: card.page.notes["when_seeded"] || ""
        color: card.page.errorColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}

// The Startup card of uQbit's uQ Settings page: opening uQbit at login (in the tray or not),
// what closing and minimising the window do, and the engine's own start at login.
import QtQuick
import QtQuick.Layouts
import org.uq.settings
import "logic.js" as Logic

SettingsCard {
    id: card
    property var page
    title: "Startup and tray"
    iconName: "system-run"

    function on(key) { return card.page.value(key) === true; }
    function set(key, v) { card.page.setKey(key, Logic.boolArg(v)); }

    SwitchRow {
        settingId: "launch_on_login"
        label: "Open uQbit when you log in"
        highlight: card.page.highlight
        value: card.on("launch_on_login")
        enabled: card.page.ready
        onChosen: v => card.set("launch_on_login", v)
        HelpTip { tip: "Adds uQbit to your startup apps (uQ Settings › Startup lists it too)." }
    }
    SwitchRow {
        settingId: "start_in_tray"
        label: "Start in the tray, with the window closed"
        highlight: card.page.highlight
        value: card.on("start_in_tray")
        enabled: card.page.ready && card.on("launch_on_login")
        onChosen: v => card.set("start_in_tray", v)
    }
    SwitchRow {
        settingId: "close_to_tray"
        label: "Closing the window keeps uQbit in the tray"
        highlight: card.page.highlight
        value: card.on("close_to_tray")
        enabled: card.page.ready
        onChosen: v => card.set("close_to_tray", v)
        HelpTip { tip: "Quit from the tray icon's menu, or with Ctrl+Q. With no tray to hold it, closing quits as usual." }
    }
    SwitchRow {
        settingId: "minimize_to_tray"
        label: "Minimising the window hides it in the tray"
        highlight: card.page.highlight
        value: card.on("minimize_to_tray")
        enabled: card.page.ready
        onChosen: v => card.set("minimize_to_tray", v)
    }
    UqLabel {
        text: Logic.trayLine(card.page.config)
        color: card.page.mutedColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    SwitchRow {
        settingId: "start_on_login"
        label: "Start the torrent engine when you log in"
        highlight: card.page.highlight
        value: card.on("start_on_login")
        enabled: card.page.ready && card.page.engine.canLogin
        onChosen: v => card.set("start_on_login", v)
        HelpTip { tip: "Torrents resume seeding without opening uQbit. Uses systemd's user service uqbit-engine." }
    }
    SwitchRow {
        settingId: "keep_running"
        label: "Keep downloading and seeding after quitting uQbit"
        highlight: card.page.highlight
        value: card.on("keep_running")
        enabled: card.page.ready
        onChosen: v => card.set("keep_running", v)
    }
    UqLabel {
        visible: text !== ""
        text: ["launch_on_login", "start_in_tray", "close_to_tray", "minimize_to_tray", "start_on_login", "keep_running"]
              .map(k => card.page.notes[k] || "").find(t => t !== "") || ""
        color: card.page.errorColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}

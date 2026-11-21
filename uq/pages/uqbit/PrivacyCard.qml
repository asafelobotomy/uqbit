// The Privacy card of uQbit's uQ Settings page: the VPN kill switch and its state, encryption,
// anonymous mode and the peer blocklist. Every change goes through the page's setKey().
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.uq.settings
import "logic.js" as Logic

SettingsCard {
    id: card
    property var page
    title: "Privacy"
    iconName: "security-high"

    readonly property var line: Logic.privacyLine(page.status)
    readonly property color okColor: (page.ctx && page.ctx.theme.colors && page.ctx.theme.colors.success) || "green"
    readonly property color warnColor: (page.ctx && page.ctx.theme.colors && page.ctx.theme.colors.warning) || "orange"

    UqLabel {
        visible: card.line.text !== ""
        text: card.line.text
        color: card.line.tone === "ok" ? card.okColor : card.line.tone === "warn" ? card.warnColor : card.page.mutedColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    SwitchRow {
        settingId: "vpn_only"
        label: "Send torrents only through the VPN"
        highlight: card.page.highlight
        value: card.page.value("vpn_only") === true
        enabled: card.page.ready
        onChosen: v => card.page.setKey("vpn_only", Logic.boolArg(v))
        HelpTip { tip: "A kill switch: the engine is bound to the VPN's connection, so when the VPN drops, torrents stop instead of using your normal connection. Searches still go straight to the sites." }
    }
    SettingRow {
        settingId: "vpn_interface"
        visible: card.page.value("vpn_only") === true
        label: "VPN connection (empty: whichever is connected)"
        highlight: card.page.highlight
        QQC2.TextField {
            id: iface
            placeholderText: "Detect"
            text: card.page.value("vpn_interface") || ""
            Layout.preferredWidth: 140
            onAccepted: saveIface.clicked()
        }
        QQC2.Button {
            id: saveIface
            text: "Save"
            enabled: card.page.workingKey === "" && Logic.cleanInterface(iface.text) !== null
                     && Logic.cleanInterface(iface.text) !== (card.page.value("vpn_interface") || "")
            onClicked: card.page.setKey("vpn_interface", Logic.cleanInterface(iface.text))
        }
    }
    SwitchRow {
        settingId: "encryption"
        label: "Connect only to peers that encrypt"
        highlight: card.page.highlight
        value: card.page.value("encryption") === "require"
        enabled: card.page.ready
        onChosen: v => card.page.setKey("encryption", v ? "require" : "prefer")
    }
    SwitchRow {
        settingId: "anonymous_mode"
        label: "Anonymous mode"
        highlight: card.page.highlight
        value: card.page.value("anonymous_mode") === true
        enabled: card.page.ready
        onChosen: v => card.page.setKey("anonymous_mode", Logic.boolArg(v))
        HelpTip { tip: "Peers and trackers aren't told which app you use or your local port." }
    }
    SwitchRow {
        settingId: "blocklist"
        label: "Block known monitoring and anti-piracy peers"
        highlight: card.page.highlight
        value: card.page.value("blocklist") === true
        enabled: card.page.ready
        onChosen: v => card.page.setKey("blocklist", Logic.boolArg(v))
    }
    RowLayout {
        visible: card.page.value("blocklist") === true
        Layout.fillWidth: true
        UqLabel {
            text: card.line.blocklist
            color: card.page.mutedColor
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        QQC2.Button {
            text: "Update now"
            enabled: card.page.workingKey === ""
            onClicked: card.page.action("blocklist-update", "blocklist")
        }
    }
    UqLabel {
        visible: text !== ""
        text: card.page.notes["vpn_only"] || card.page.notes["vpn_interface"] || card.page.notes["encryption"]
              || card.page.notes["anonymous_mode"] || card.page.notes["blocklist"] || ""
        color: card.page.errorColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}

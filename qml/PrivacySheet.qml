import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import Uqbit
import "logic.js" as Logic

// Privacy: whether the VPN protects the torrents right now, and the switches behind it.
Popup {
    id: sheet
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, parent ? parent.width - 40 : 560)
    padding: 20
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property var p: App.protection
    readonly property var said: Logic.protection(p)
    readonly property var list: p.blocklist || {}

    function set(key, value) {
        const e = App.setSetting(key, String(value));
        if (e !== "")
            App.notice(e);
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius + 2
        border.color: Theme.border
    }

    component SettingRow: RowLayout {
        id: r
        property string title
        property string detail
        default property alias control: holder.data
        Layout.fillWidth: true
        spacing: 12
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                text: r.title
                color: Theme.text
                font.family: Theme.fontUi
                font.pointSize: Theme.bodySize
            }
            Label {
                Layout.fillWidth: true
                text: r.detail
                visible: text !== ""
                wrapMode: Text.Wrap
                color: Theme.textMuted
                font.family: Theme.fontUi
                font.pointSize: Theme.smallSize
            }
        }
        Item {
            id: holder
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        RowLayout {
            Label {
                Layout.fillWidth: true
                text: qsTr("Privacy")
                color: Theme.text
                font.family: Theme.fontUi
                font.pointSize: Theme.bodySize + 3
                font.weight: Font.DemiBold
            }
            IconButton {
                size: 32
                iconName: "close"
                tip: qsTr("Close")
                onClicked: sheet.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: status.implicitHeight + 20
            radius: Theme.radius
            readonly property color tone: sheet.said.level === "ok" ? Theme.success : sheet.said.level === "warn" ? Theme.warning : Theme.textMuted
            color: Qt.alpha(tone, 0.1)
            border.color: Qt.alpha(tone, 0.4)
            RowLayout {
                id: status
                anchors.fill: parent
                anchors.margins: 10
                anchors.leftMargin: 14
                spacing: 12
                Impl.IconImage {
                    source: "qrc:/qt/qml/Uqbit/icons/" + sheet.said.icon + ".svg"
                    sourceSize: Qt.size(26, 26)
                    color: parent.parent.tone
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: sheet.said.title
                        color: Theme.text
                        font.family: Theme.fontUi
                        font.pointSize: Theme.bodySize
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: sheet.said.detail
                        wrapMode: Text.Wrap
                        color: Theme.textMuted
                        font.family: Theme.fontUi
                        font.pointSize: Theme.smallSize
                    }
                }
            }
        }

        SettingRow {
            title: qsTr("Only through the VPN")
            detail: qsTr("Bind the engine to the VPN's connection. When the VPN drops, torrents stop instead of using your normal connection.")
            ThemedSwitch {
                checked: App.settings.vpn_only === true
                Accessible.name: qsTr("Only through the VPN")
                onToggled: sheet.set("vpn_only", checked)
            }
        }
        SettingRow {
            visible: App.settings.vpn_only === true
            title: qsTr("VPN connection")
            detail: qsTr("Leave empty to use whichever VPN is connected (WireGuard, OpenVPN, Proton VPN…).")
            TextField {
                id: iface
                width: 150
                text: App.settings.vpn_interface || ""
                placeholderText: sheet.p.interface && sheet.p.up ? sheet.p.interface : qsTr("Detect")
                color: Theme.text
                font.family: Theme.fontUi
                selectByMouse: true
                validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9_.:@-]{0,15}/ }
                onEditingFinished: if (text !== (App.settings.vpn_interface || "")) sheet.set("vpn_interface", text)
                background: Rectangle {
                    implicitHeight: 34
                    color: Theme.bg
                    radius: Theme.radius
                    border.color: iface.activeFocus ? Theme.accent : Qt.alpha(Theme.border, 0.6)
                }
            }
        }
        SettingRow {
            title: qsTr("Encrypted connections only")
            detail: qsTr("Talk only to peers that encrypt, so your network can't see what you share.")
            ThemedSwitch {
                checked: App.settings.encryption === "require"
                Accessible.name: qsTr("Encrypted connections only")
                onToggled: sheet.set("encryption", checked ? "require" : "prefer")
            }
        }
        SettingRow {
            title: qsTr("Anonymous mode")
            detail: qsTr("Don't tell peers and trackers which app you use or your local port.")
            ThemedSwitch {
                checked: App.settings.anonymous_mode === true
                Accessible.name: qsTr("Anonymous mode")
                onToggled: sheet.set("anonymous_mode", checked)
            }
        }
        SettingRow {
            title: qsTr("Block known monitoring peers")
            detail: !sheet.list.on ? qsTr("A public list of anti-piracy and monitoring addresses.")
                    : sheet.list.updating ? qsTr("Downloading the list…")
                    : sheet.list.error ? sheet.list.error
                    : sheet.list.loaded ? qsTr("List updated %1. It refreshes every week.").arg(Logic.date(Date.parse(sheet.list.updated) / 1000))
                    : qsTr("The list hasn't been downloaded yet.")
            RowLayout {
                spacing: 6
                IconButton {
                    size: 32
                    iconName: "refresh"
                    tip: qsTr("Update the list now")
                    visible: sheet.list.on === true
                    enabled: !sheet.list.updating && App.status === "ready"
                    onClicked: App.updateBlocklist()
                }
                ThemedSwitch {
                    checked: sheet.list.on === true
                    Accessible.name: qsTr("Block known monitoring peers")
                    onToggled: sheet.set("blocklist", checked)
                }
            }
        }
    }
}

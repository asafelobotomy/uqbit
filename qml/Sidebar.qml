import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import Uqbit
import "logic.js" as Logic

// The left column: add buttons, the filters with their counts, and the totals at the bottom.
Rectangle {
    id: side
    signal addFiles()
    signal addLink()
    signal privacyRequested()
    /// "torrents" or "search": which one the main area shows.
    property string view: "torrents"
    signal viewRequested(string view)
    color: Theme.surface

    readonly property var server: App.torrents.server
    readonly property var counts: App.torrents.counts

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 6

        RowLayout {
            Layout.bottomMargin: 10
            spacing: 10
            Image {
                source: "qrc:/qt/qml/Uqbit/icons/uqbit.svg"
                sourceSize: Qt.size(30, 30)
            }
            Label {
                text: "uQbit"
                color: Theme.text
                font.family: Theme.fontUi
                font.pointSize: Theme.bodySize + 4
                font.weight: Font.DemiBold
            }
        }

        PillButton {
            Layout.fillWidth: true
            primary: true
            iconName: "add"
            text: qsTr("Open torrent")
            enabled: App.status === "ready"
            onClicked: side.addFiles()
        }
        PillButton {
            Layout.fillWidth: true
            iconName: "magnet"
            text: qsTr("Add magnet link")
            enabled: App.status === "ready"
            onClicked: side.addLink()
        }

        Item { Layout.preferredHeight: 10 }

        Repeater {
            model: Logic.filters
            delegate: AbstractButton {
                id: f
                required property var modelData
                readonly property bool current: side.view === "torrents" && App.torrents.filter === modelData.id
                readonly property int n: side.counts[modelData.id] || 0
                Layout.fillWidth: true
                implicitHeight: 36
                hoverEnabled: true
                focusPolicy: Qt.TabFocus
                Accessible.name: modelData.label
                onClicked: { App.torrents.filter = modelData.id; side.viewRequested("torrents"); }
                background: Rectangle {
                    radius: Theme.radius
                    color: f.current ? Qt.alpha(Theme.accent, 0.16) : f.hovered ? Qt.alpha(Theme.text, 0.06) : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                    border.color: f.visualFocus ? Theme.accent : "transparent"
                }
                contentItem: RowLayout {
                    spacing: 10
                    Impl.IconImage {
                        Layout.leftMargin: 8
                        source: "qrc:/qt/qml/Uqbit/icons/" + f.modelData.icon + ".svg"
                        sourceSize: Qt.size(18, 18)
                        color: f.current ? Theme.accent : Theme.textMuted
                    }
                    Label {
                        Layout.fillWidth: true
                        text: f.modelData.label
                        color: f.current ? Theme.text : Theme.textMuted
                        font.family: Theme.fontUi
                        font.pointSize: Theme.bodySize
                        font.weight: f.current ? Font.DemiBold : Font.Normal
                    }
                    Label {
                        Layout.rightMargin: 10
                        visible: f.n > 0
                        text: f.n
                        color: f.modelData.id === "error" ? Theme.error : Theme.textMuted
                        font.family: Theme.fontUi
                        font.pointSize: Theme.smallSize
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            implicitHeight: 1
            color: Qt.alpha(Theme.border, 0.5)
        }

        AbstractButton {
            id: searchButton
            readonly property bool current: side.view === "search"
            Layout.fillWidth: true
            implicitHeight: 36
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            Accessible.name: qsTr("Search")
            onClicked: side.viewRequested("search")
            background: Rectangle {
                radius: Theme.radius
                color: searchButton.current ? Qt.alpha(Theme.accent, 0.16) : searchButton.hovered ? Qt.alpha(Theme.text, 0.06) : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                border.color: searchButton.visualFocus ? Theme.accent : "transparent"
            }
            contentItem: RowLayout {
                spacing: 10
                Impl.IconImage {
                    Layout.leftMargin: 8
                    source: "qrc:/qt/qml/Uqbit/icons/search.svg"
                    sourceSize: Qt.size(18, 18)
                    color: searchButton.current ? Theme.accent : Theme.textMuted
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Search")
                    color: searchButton.current ? Theme.text : Theme.textMuted
                    font.family: Theme.fontUi
                    font.pointSize: Theme.bodySize
                    font.weight: searchButton.current ? Font.DemiBold : Font.Normal
                }
                BusyIndicator {
                    Layout.rightMargin: 6
                    implicitWidth: 18
                    implicitHeight: 18
                    running: App.searchState === "running"
                    visible: running
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Privacy: is the VPN protecting the torrents?
        AbstractButton {
            id: shield
            readonly property var said: Logic.protection(App.protection)
            readonly property color tone: said.level === "ok" ? Theme.success : said.level === "warn" ? Theme.warning : Theme.textMuted
            Layout.fillWidth: true
            implicitHeight: shieldRow.implicitHeight + 16
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            Accessible.name: said.title
            onClicked: side.privacyRequested()
            background: Rectangle {
                radius: Theme.radius
                color: shield.hovered ? Qt.alpha(shield.tone, 0.16) : Qt.alpha(shield.tone, 0.08)
                Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                border.color: shield.visualFocus ? Theme.accent : Qt.alpha(shield.tone, 0.35)
            }
            contentItem: RowLayout {
                id: shieldRow
                spacing: 8
                Impl.IconImage {
                    Layout.leftMargin: 8
                    source: "qrc:/qt/qml/Uqbit/icons/" + shield.said.icon + ".svg"
                    sourceSize: Qt.size(18, 18)
                    color: shield.tone
                }
                Label {
                    Layout.fillWidth: true
                    Layout.rightMargin: 8
                    text: shield.said.title
                    elide: Text.ElideRight
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pointSize: Theme.smallSize
                }
            }
        }

        // Totals
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: totals.implicitHeight + 20
            radius: Theme.radius
            color: Theme.surfaceAlt
            visible: App.status === "ready"
            ColumnLayout {
                id: totals
                anchors.fill: parent
                anchors.margins: 10
                spacing: 4
                RowLayout {
                    spacing: 6
                    Impl.IconImage { source: "qrc:/qt/qml/Uqbit/icons/down.svg"; sourceSize: Qt.size(14, 14); color: Theme.accent }
                    Label {
                        Layout.fillWidth: true
                        text: Logic.speed(side.server.dl_info_speed) || "0 B/s"
                        color: Theme.text
                        font.family: Theme.fontUi
                        font.pointSize: Theme.smallSize
                    }
                    Impl.IconImage { source: "qrc:/qt/qml/Uqbit/icons/up.svg"; sourceSize: Qt.size(14, 14); color: Theme.success }
                    Label {
                        Layout.fillWidth: true
                        text: Logic.speed(side.server.up_info_speed) || "0 B/s"
                        color: Theme.text
                        font.family: Theme.fontUi
                        font.pointSize: Theme.smallSize
                    }
                }
                Label {
                    visible: side.server.free_space_on_disk !== undefined
                    text: qsTr("%1 free").arg(Logic.bytes(side.server.free_space_on_disk))
                    color: Theme.textMuted
                    font.family: Theme.fontUi
                    font.pointSize: Theme.smallSize
                }
                Label {
                    text: side.server.connection_status === "firewalled" ? qsTr("Firewalled: fewer peers can reach you")
                          : side.server.connection_status === "disconnected" ? qsTr("Not connected")
                          : qsTr("qBittorrent %1").arg(App.engineVersion.replace(/^v/, ""))
                    color: side.server.connection_status === "connected" ? Theme.textMuted : Theme.warning
                    font.family: Theme.fontUi
                    font.pointSize: Theme.smallSize
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}

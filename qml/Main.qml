import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import QtQuick.Dialogs
import QtQuick.Layouts
import Uqbit
import "logic.js" as Logic

ApplicationWindow {
    id: win
    width: 1180
    height: 720
    minimumWidth: 640
    minimumHeight: 400
    visible: true
    title: Logic.windowTitle(App.torrents.server.dl_info_speed, App.torrents.server.up_info_speed)
    color: Theme.bg

    readonly property bool ready: App.status === "ready"
    readonly property bool wide: width >= 980

    palette.window: Theme.surface
    palette.windowText: Theme.text
    palette.base: Theme.bg
    palette.text: Theme.text
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    palette.light: Theme.overlay
    palette.mid: Theme.border
    palette.dark: Theme.border
    palette.toolTipBase: Theme.overlay
    palette.toolTipText: Theme.text
    palette.placeholderText: Theme.textMuted
    font.family: Theme.fontUi

    Connections {
        target: App
        function onNotice(text) { toast.show(text); }
        function onAddRequested() { if (!addSheet.opened) addSheet.open(); }
    }

    function selectedName() { return (App.torrents.get(App.selected).name) || ""; }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            Layout.fillHeight: true
            Layout.preferredWidth: 230
            onAddFiles: fileDialog.open()
            onAddLink: linkDialog.open()
        }

        // ---- the list
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 16
            spacing: 12

            RowLayout {
                spacing: 10
                Label {
                    text: Logic.filters.find(f => f.id === App.torrents.filter).label
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pointSize: Theme.headingSize
                    font.weight: Font.DemiBold
                }
                Item { Layout.fillWidth: true }
                TextField {
                    id: search
                    Layout.preferredWidth: Math.min(280, win.width * 0.25)
                    placeholderText: qsTr("Search names")
                    leftPadding: 34
                    color: Theme.text
                    font.family: Theme.fontUi
                    selectByMouse: true
                    visible: App.torrents.total > 0
                    onTextChanged: App.torrents.search = text
                    Keys.onEscapePressed: { text = ""; win.contentItem.forceActiveFocus(); }
                    background: Rectangle {
                        implicitHeight: 36
                        color: Theme.surface
                        radius: Theme.radius
                        border.color: search.activeFocus ? Theme.accent : Qt.alpha(Theme.border, 0.6)
                        Impl.IconImage {
                            anchors.verticalCenter: parent.verticalCenter
                            x: 10
                            source: "qrc:/qt/qml/Uqbit/icons/search.svg"
                            sourceSize: Qt.size(16, 16)
                            color: Theme.textMuted
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: list
                    anchors.fill: parent
                    visible: win.ready && count > 0
                    clip: true
                    spacing: 8
                    model: App.torrents
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    delegate: TorrentRow {
                        width: ListView.view.width - 4
                        onMenuRequested: (x, y) => rowMenu.popup()
                    }
                }

                EmptyState {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 40, 460)
                    visible: !list.visible
                    busy: App.status === "starting"
                    icon: App.status === "ready" ? (App.torrents.total === 0 ? "uqbit" : "search")
                          : App.status === "starting" ? "uqbit" : "error"
                    title: App.status === "starting" ? qsTr("Starting the torrent engine")
                           : App.status === "missing" ? qsTr("The torrent engine is missing")
                           : App.status === "error" ? qsTr("The torrent engine isn't answering")
                           : App.torrents.total === 0 ? qsTr("No torrents yet") : qsTr("Nothing here")
                    detail: App.status === "ready" ? Logic.emptyText(App.torrents.filter, App.torrents.search !== "")
                            : App.status === "missing" ? qsTr("uQbit runs qBittorrent's engine in the background. Install qbittorrent-nox (or qbittorrent-enhanced-nox), then try again.")
                            : App.statusText

                    PillButton {
                        visible: App.status === "ready" && App.torrents.total === 0
                        primary: true
                        iconName: "add"
                        text: qsTr("Open torrent")
                        onClicked: fileDialog.open()
                    }
                    PillButton {
                        visible: App.status === "ready" && App.torrents.total === 0
                        iconName: "magnet"
                        text: qsTr("Add magnet link")
                        onClicked: linkDialog.open()
                    }
                    PillButton {
                        visible: App.status === "error" || App.status === "missing"
                        primary: true
                        iconName: "refresh"
                        text: qsTr("Try again")
                        onClicked: App.retry()
                    }
                }
            }

            // ---- default handler nudge
            Rectangle {
                Layout.fillWidth: true
                visible: win.ready && !App.isDefault && !nudgeClosed
                property bool nudgeClosed: false
                id: nudge
                implicitHeight: nudgeRow.implicitHeight + 16
                radius: Theme.radius
                color: Qt.alpha(Theme.accent, 0.1)
                border.color: Qt.alpha(Theme.accent, 0.35)
                RowLayout {
                    id: nudgeRow
                    anchors.fill: parent
                    anchors.margins: 8
                    anchors.leftMargin: 14
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Open magnet links and .torrent files with uQbit?")
                        color: Theme.text
                        font.family: Theme.fontUi
                        font.pointSize: Theme.bodySize
                        wrapMode: Text.Wrap
                    }
                    PillButton {
                        primary: true
                        text: qsTr("Make default")
                        onClicked: App.makeDefault()
                    }
                    IconButton {
                        size: 32
                        iconName: "close"
                        tip: qsTr("Not now")
                        onClicked: nudge.nudgeClosed = true
                    }
                }
            }
        }

        DetailsPanel {
            Layout.fillHeight: true
            Layout.preferredWidth: win.wide ? 360 : 300
            Layout.topMargin: 16
            Layout.bottomMargin: 16
            Layout.rightMargin: 16
            visible: App.selected !== "" && win.ready
            onRemoveRequested: (hash, name) => removeDialog.ask(hash, name)
        }
    }

    Menu {
        id: rowMenu
        readonly property var t: App.torrents.get(App.selected)
        readonly property bool running: Logic.isRunning(t.group)
        MenuItem {
            text: rowMenu.running ? qsTr("Stop") : qsTr("Start")
            onTriggered: App.setRunning([App.selected], !rowMenu.running)
        }
        MenuItem { text: qsTr("Open folder"); onTriggered: App.openFolder(App.selected) }
        MenuItem { text: qsTr("Copy magnet link"); onTriggered: App.copyMagnet(App.selected) }
        MenuItem { text: qsTr("Check the files again"); onTriggered: App.recheck(App.selected) }
        MenuItem { text: qsTr("Ask for more peers"); onTriggered: App.reannounce(App.selected) }
        MenuSeparator {}
        MenuItem { text: qsTr("Remove…"); onTriggered: removeDialog.ask(App.selected, win.selectedName()) }
    }

    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 84
        z: 10
    }

    AddSheet { id: addSheet }
    RemoveDialog { id: removeDialog }
    LinkDialog { id: linkDialog }

    FileDialog {
        id: fileDialog
        title: qsTr("Open torrent files")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Torrent files (*.torrent)"), qsTr("All files (*)")]
        onAccepted: App.openInputs(selectedFiles.map(u => u.toString()))
    }

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list", "text/plain"]
        onEntered: drag => { dropHint.visible = Logic.torrentInputs(drag.urls.map(u => u.toString()).concat([drag.text || ""])).length > 0; }
        onExited: dropHint.visible = false
        onDropped: drop => {
            dropHint.visible = false;
            const items = Logic.torrentInputs(drop.urls.map(u => u.toString()).concat([drop.text || ""]));
            if (items.length > 0)
                App.openInputs(items);
            else
                toast.show(qsTr("That's not a torrent file or a magnet link"));
        }
    }
    Rectangle {
        id: dropHint
        anchors.fill: parent
        anchors.margins: 10
        visible: false
        radius: Theme.radius + 4
        color: Qt.alpha(Theme.accent, 0.08)
        border.color: Theme.accent
        border.width: 2
        Label {
            anchors.centerIn: parent
            text: qsTr("Drop to add")
            color: Theme.accent
            font.family: Theme.fontUi
            font.pointSize: Theme.headingSize
        }
    }

    // ---- keys
    Shortcut { sequences: [StandardKey.Open]; enabled: win.ready; onActivated: fileDialog.open() }
    Shortcut { sequence: "Ctrl+L"; enabled: win.ready; onActivated: linkDialog.open() }
    Shortcut {
        sequences: [StandardKey.Paste]
        enabled: win.ready && !search.activeFocus
        onActivated: {
            if (App.pasteLinks() === 0)
                toast.show(qsTr("No magnet links on the clipboard"));
        }
    }
    Shortcut { sequences: [StandardKey.Find]; onActivated: search.forceActiveFocus() }
    Shortcut { sequence: "Escape"; enabled: App.selected !== "" && !search.activeFocus; onActivated: App.selected = "" }
    Shortcut {
        sequence: "Delete"
        enabled: App.selected !== "" && !search.activeFocus
        onActivated: removeDialog.ask(App.selected, win.selectedName())
    }
    Shortcut {
        sequence: "Space"
        enabled: App.selected !== "" && !search.activeFocus
        onActivated: App.setRunning([App.selected], !Logic.isRunning(App.torrents.get(App.selected).group))
    }
    Shortcut { sequences: ["Ctrl+1"]; onActivated: App.torrents.filter = "all" }
    Shortcut { sequences: ["Ctrl+2"]; onActivated: App.torrents.filter = "downloading" }
    Shortcut { sequences: ["Ctrl+3"]; onActivated: App.torrents.filter = "seeding" }
    Shortcut { sequences: ["Ctrl+4"]; onActivated: App.torrents.filter = "finished" }
    Shortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }
}

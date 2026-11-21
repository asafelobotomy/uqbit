import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import Uqbit
import "logic.js" as Logic

// The selected torrent: what to do with it, its numbers, its files and trackers.
Rectangle {
    id: panel
    signal removeRequested(string hash, string name)

    property var t: ({})
    readonly property var props: App.details.props || ({})
    readonly property var files: App.details.files || []
    readonly property var trackers: (App.details.trackers || []).filter(x => !/^\*\* \[/.test(x.url))
    readonly property bool running: Logic.isRunning(t.group)

    function refresh() {
        const m = App.torrents.get(App.selected);
        t = {
            name: m.name || "", state: m.state || "", group: m.group || "",
            progress: m.progress || 0, size: m.size || 0, ratio: m.ratio || 0,
            uploaded: m.uploaded || 0, downloaded: m.downloaded || 0, seeds: m.num_seeds || 0,
            peers: m.num_leechs || 0, savePath: m.save_path || "", added: m.added_on || 0,
            magnet: m.magnet_uri || ""
        };
    }

    Connections {
        target: App.torrents
        function onCountsChanged() { panel.refresh(); }
    }
    Connections {
        target: App
        function onSelectedChanged() { panel.refresh(); }
    }
    Component.onCompleted: refresh()

    color: Theme.surface
    radius: Theme.radius
    border.color: Qt.alpha(Theme.border, 0.5)

    component Fact: RowLayout {
        property alias key: k.text
        property alias value: v.text
        Layout.fillWidth: true
        spacing: 8
        Label {
            id: k
            Layout.preferredWidth: 96
            color: Theme.textMuted
            font.family: Theme.fontUi
            font.pointSize: Theme.smallSize
        }
        Label {
            id: v
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            color: Theme.text
            font.family: Theme.fontUi
            font.pointSize: Theme.smallSize
        }
    }
    component Heading: Label {
        Layout.topMargin: 10
        color: Theme.text
        font.family: Theme.fontUi
        font.pointSize: Theme.bodySize
        font.weight: Font.DemiBold
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Label {
                Layout.fillWidth: true
                text: panel.t.name
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.fontUi
                font.pointSize: Theme.bodySize + 1
                font.weight: Font.DemiBold
            }
            IconButton {
                Layout.alignment: Qt.AlignTop
                size: 32
                iconName: "close"
                tip: qsTr("Close (Esc)")
                onClicked: App.selected = ""
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            PillButton {
                iconName: panel.running ? "pause" : "play"
                text: panel.running ? qsTr("Stop") : qsTr("Start")
                onClicked: App.setRunning([App.selected], !panel.running)
            }
            IconButton {
                size: 36
                iconName: "folder"
                tip: qsTr("Open its folder")
                onClicked: App.openFolder(App.selected)
            }
            IconButton {
                size: 36
                iconName: "magnet"
                tip: qsTr("Copy magnet link")
                visible: panel.t.magnet !== ""
                onClicked: App.copyMagnet(App.selected)
            }
            IconButton {
                size: 36
                iconName: "refresh"
                tip: qsTr("Check the files again")
                onClicked: App.recheck(App.selected)
            }
            IconButton {
                size: 36
                iconName: "remove"
                tip: qsTr("Remove (Delete)")
                tint: Theme.error
                onClicked: panel.removeRequested(App.selected, panel.t.name)
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: body.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: body
                width: parent.width
                spacing: 6

                Heading { text: qsTr("Details"); Layout.topMargin: 4 }
                Fact { key: qsTr("Status"); value: Logic.stateLabel(panel.t.state, panel.t.progress) + " · " + Logic.percent(panel.t.progress) }
                Fact { key: qsTr("Size"); value: Logic.bytes(panel.t.size) }
                Fact { key: qsTr("Downloaded"); value: Logic.bytes(panel.t.downloaded) }
                Fact { key: qsTr("Uploaded"); value: Logic.bytes(panel.t.uploaded) + " (ratio " + Logic.ratio(panel.t.ratio) + ")" }
                Fact { key: qsTr("Peers"); value: qsTr("%1 seeds, %2 others").arg(panel.t.seeds).arg(panel.t.peers) }
                Fact { key: qsTr("Pieces"); value: panel.props.pieces_num ? qsTr("%1 × %2").arg(panel.props.pieces_num).arg(Logic.bytes(panel.props.piece_size)) : "" }
                Fact { key: qsTr("Saved in"); value: App.prettyFolder(panel.t.savePath) }
                Fact { key: qsTr("Added"); value: Logic.date(panel.t.added) }
                Fact { key: qsTr("Hash"); value: App.selected }

                Heading { text: panel.files.length === 1 ? qsTr("1 file") : qsTr("%1 files").arg(panel.files.length) }
                Repeater {
                    model: panel.files.slice(0, 300)
                    delegate: RowLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 8
                        ThemedCheck {
                            id: want
                            checked: modelData.priority > 0
                            onToggled: App.setFileWanted(modelData.index !== undefined ? modelData.index : index, checked)
                            ToolTip.visible: hovered
                            ToolTip.delay: 600
                            ToolTip.text: checked ? qsTr("Download this file") : qsTr("Skip this file")
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: Logic.baseName(modelData.name)
                                elide: Text.ElideMiddle
                                color: want.checked ? Theme.text : Theme.textMuted
                                font.family: Theme.fontUi
                                font.pointSize: Theme.smallSize
                            }
                            ProgressLine {
                                Layout.fillWidth: true
                                implicitHeight: 3
                                value: modelData.progress
                                visible: want.checked
                            }
                        }
                        Label {
                            text: Logic.bytes(modelData.size)
                            color: Theme.textMuted
                            font.family: Theme.fontUi
                            font.pointSize: Theme.smallSize
                        }
                    }
                }

                Heading { text: qsTr("Trackers"); visible: panel.trackers.length > 0 }
                Repeater {
                    model: panel.trackers.slice(0, 30)
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: modelData.url
                            elide: Text.ElideMiddle
                            color: Theme.text
                            font.family: Theme.fontUi
                            font.pointSize: Theme.smallSize
                        }
                        Label {
                            text: Logic.trackerStatus(modelData.status) + (modelData.msg ? " · " + modelData.msg : "")
                            color: modelData.status === 4 ? Theme.error : Theme.textMuted
                            font.family: Theme.fontUi
                            font.pointSize: Theme.smallSize - 0.5
                        }
                    }
                }
                PillButton {
                    visible: panel.trackers.length > 0
                    Layout.topMargin: 4
                    text: qsTr("Ask for more peers")
                    onClicked: App.reannounce(App.selected)
                }
            }
        }
    }
}

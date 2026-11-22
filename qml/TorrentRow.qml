import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import Uqbit
import "logic.js" as Logic

// One torrent in the list: name, progress, what it's doing, and a start/stop button.
AbstractButton {
    id: row
    required property string hash
    required property string name
    required property string state
    required property string group
    required property real progress
    required property double size
    required property double downloaded
    required property double dlspeed
    required property double upspeed
    required property double eta
    required property real ratio
    required property int index
    readonly property bool current: App.selected === hash
    readonly property string tone: Logic.stateTone(group, state)
    readonly property color toneColor: tone === "error" ? Theme.error : tone === "success" ? Theme.success
                                       : tone === "warning" ? Theme.warning : tone === "muted" ? Theme.textMuted : Theme.accent

    signal menuRequested(real x, real y)

    implicitHeight: 74
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    Accessible.name: name
    onClicked: App.selected = current ? "" : hash
    onDoubleClicked: App.openFolder(hash)

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (point) => { App.selected = row.hash; row.menuRequested(point.position.x, point.position.y); }
    }

    background: Rectangle {
        radius: Theme.radius
        color: row.current ? Qt.alpha(Theme.accent, 0.13) : row.hovered ? Qt.alpha(Theme.text, 0.05) : Theme.surface
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        border.color: row.current ? Qt.alpha(Theme.accent, 0.55) : Qt.alpha(Theme.border, 0.35)
        border.width: 1
    }

    contentItem: RowLayout {
        spacing: 12
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            spacing: 6
            RowLayout {
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: row.name
                    elide: Text.ElideMiddle
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pointSize: Theme.bodySize
                    font.weight: Font.DemiBold
                }
                Rectangle {
                    implicitWidth: stateLabel.implicitWidth + 16
                    implicitHeight: stateLabel.implicitHeight + 6
                    radius: height / 2
                    color: Qt.alpha(row.toneColor, 0.15)
                    Label {
                        id: stateLabel
                        anchors.centerIn: parent
                        text: Logic.stateLabel(row.state, row.progress)
                        color: row.toneColor
                        font.family: Theme.fontUi
                        font.pointSize: Theme.smallSize
                    }
                }
            }
            ProgressLine {
                Layout.fillWidth: true
                value: row.progress
                tint: row.toneColor
            }
            RowLayout {
                Label {
                    Layout.fillWidth: true
                    text: Logic.summary({ progress: row.progress, size: row.size, downloaded: row.downloaded,
                                          dlspeed: row.dlspeed, upspeed: row.upspeed, eta: row.eta, ratio: row.ratio })
                    elide: Text.ElideRight
                    color: Theme.textMuted
                    font.family: Theme.fontUi
                    font.pointSize: Theme.smallSize
                }
                Label {
                    text: Logic.percent(row.progress)
                    color: Theme.textMuted
                    font.family: Theme.fontUi
                    font.pointSize: Theme.smallSize
                }
            }
        }
        IconButton {
            Layout.rightMargin: 8
            size: 36
            iconName: Logic.isRunning(row.group) ? "pause" : "play"
            tip: Logic.isRunning(row.group) ? qsTr("Stop") : qsTr("Start")
            onClicked: App.setRunning([row.hash], !Logic.isRunning(row.group))
        }
    }
}

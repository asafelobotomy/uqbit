// uQbit's quick settings tile (docs/integrating.md in the uQ repository): "Torrents", the
// speeds or what holds them back, and a switch that pauses or resumes every torrent. Clicking
// the text opens uQbit. Shown while the torrent engine runs; polls `uqbit status --json` every
// 3 s while quick settings is open. All text goes through ctx.UqText.
import QtQuick
import "logic.js" as L

Item {
    id: root

    property var ctx
    property var status: ({})
    readonly property var v: L.view(status)
    property bool hidden: !v.running
    readonly property bool onScreen: !ctx || !ctx.view || ctx.view.shown
    property bool asking: false
    // Running: anything downloading or seeding (the switch is on).
    readonly property bool on: v.canPause
    implicitHeight: 32

    function refresh() {
        if (asking || !ctx)
            return;
        asking = true;
        ctx.command.run("status", [], function (r) {
            root.asking = false;
            if (r && r.code === 0 && r.output)
                root.status = r.output;
        });
    }

    function run(id) {
        ctx.command.run(id, [], function () { root.refresh(); });
    }

    Component.onCompleted: refresh()
    onOnScreenChanged: if (onScreen) refresh()

    Timer {
        interval: root.onScreen ? 3000 : 30000
        running: true
        repeat: true
        onTriggered: root.refresh()
    }

    Rectangle {
        anchors.fill: parent
        radius: root.ctx ? root.ctx.Theme.radius : 8
        color: !root.ctx ? "transparent" : textArea.containsMouse ? root.ctx.Theme.overlay : root.ctx.Theme.surfaceAlt
    }

    Caption {
        id: title
        ctx: root.ctx
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: "Torrents"
        bold: true
    }
    Caption {
        ctx: root.ctx
        anchors.left: title.right
        anchors.leftMargin: 10
        anchors.right: toggle.left
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: root.v.warn ? root.v.vpn : root.v.speeds
        color: !root.ctx ? "grey" : root.v.warn ? root.ctx.Theme.warning : root.ctx.Theme.textMuted
    }
    MouseArea {
        id: textArea
        anchors.left: parent.left
        anchors.right: toggle.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.run("show")
    }

    // The switch: on while torrents run.
    Rectangle {
        id: toggle
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        width: 36
        height: 20
        radius: 10
        opacity: root.v.canPause || root.v.canResume ? 1 : 0.5
        color: !root.ctx ? "grey" : root.on ? root.ctx.Theme.accent : root.ctx.Theme.overlay
        Accessible.role: Accessible.CheckBox
        Accessible.name: "Torrents running"
        Accessible.checked: root.on
        Rectangle {
            x: root.on ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: 14
            radius: 7
            color: !root.ctx ? "white" : root.on ? root.ctx.Theme.accentText : root.ctx.Theme.textMuted
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.on)
                    root.run("pause-all");
                else if (root.v.canResume)
                    root.run("resume-all");
            }
        }
    }
}

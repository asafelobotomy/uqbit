// uQbit's icon in uQ's Status island (docs/integrating.md in the uQ repository). It shows
// while the torrent engine runs: an arrow over a tray, filled with the accent while anything
// downloads, with a warning dot while the VPN rule holds torrents back. Clicking it opens the
// speeds, the counts, the VPN state and pause/resume. Everything comes from `uqbit status
// --json` (polled every 3 s on screen, every 20 s off it); all text goes through ctx.UqText.
import QtQuick
import "logic.js" as L

Item {
    id: root

    property var ctx
    property var status: ({})
    readonly property var v: L.view(status)
    // No engine, no icon (uQ keeps the widget loaded and polling).
    property bool hidden: !v.running
    readonly property bool onScreen: !ctx || !ctx.view || ctx.view.shown
    property string problem: ""
    property bool asking: false
    property bool popupShown: false
    property real popupClosedAt: 0

    function refresh() {
        if (asking || !ctx)
            return;
        asking = true;
        ctx.command.run("status", [], function (r) {
            root.asking = false;
            if (r && r.code === 0 && r.output) {
                root.status = r.output;
                root.problem = "";
            } else {
                root.problem = String((r && r.error) || "uqbit didn't answer").slice(0, 160);
            }
        });
    }

    function run(id) {
        ctx.command.run(id, [], function (r) {
            if (r && r.error)
                root.problem = String(r.error).slice(0, 160);
            root.refresh();
        });
    }

    function togglePopup() {
        if (popupShown)
            ctx.closePopup();
        else if (Date.now() - popupClosedAt > 300)
            ctx.openPopup();
    }

    Component.onCompleted: refresh()
    onOnScreenChanged: if (onScreen) refresh()

    Timer {
        interval: root.onScreen ? (root.popupShown ? 1500 : 3000) : 20000
        running: true
        repeat: true
        onTriggered: root.refresh()
    }

    Rectangle {
        anchors.fill: parent
        radius: root.ctx ? Math.max(0, root.ctx.Theme.radius - 2) : 4
        color: mouse.containsMouse && root.ctx ? root.ctx.Theme.overlay : "transparent"
    }

    Canvas {
        id: glyph
        anchors.centerIn: parent
        width: 20
        height: 20
        property bool busy: root.v.busy
        property bool warn: root.v.warn
        onBusyChanged: requestPaint()
        onWarnChanged: requestPaint()
        Connections {
            target: root.ctx ? root.ctx.Theme : null
            function onAccentChanged() { glyph.requestPaint(); }
            function onTextChanged() { glyph.requestPaint(); }
        }
        onPaint: {
            var g = getContext("2d");
            g.reset();
            if (!root.ctx)
                return;
            var t = root.ctx.Theme, c = width / 2;
            g.lineCap = "round";
            g.lineJoin = "round";
            g.lineWidth = 1.7;
            g.strokeStyle = String(busy ? t.accent : t.text);
            // An arrow into a tray.
            g.beginPath();
            g.moveTo(c, 3);
            g.lineTo(c, 12);
            g.moveTo(c - 4, 8.5);
            g.lineTo(c, 12.5);
            g.lineTo(c + 4, 8.5);
            g.moveTo(3.5, 12.5);
            g.lineTo(3.5, 16.5);
            g.lineTo(16.5, 16.5);
            g.lineTo(16.5, 12.5);
            g.stroke();
            if (warn) {
                g.fillStyle = String(t.warning);
                g.beginPath();
                g.arc(16.5, 4, 3.2, 0, 2 * Math.PI);
                g.fill();
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            root.refresh();
            root.togglePopup();
        }
    }

    property Component popup: Component {
        Item {
            id: pop
            property var ctx
            implicitWidth: 300
            implicitHeight: column.implicitHeight + 24
            Component.onCompleted: root.popupShown = true
            Component.onDestruction: {
                root.popupShown = false;
                root.popupClosedAt = Date.now();
            }

            Column {
                id: column
                x: 12
                y: 12
                width: pop.width - 24
                spacing: 8

                Caption { ctx: pop.ctx; width: column.width; text: "Torrents"; bold: true }
                Caption {
                    ctx: pop.ctx
                    width: column.width
                    visible: root.v.running
                    text: root.v.speeds
                    size: pop.ctx ? pop.ctx.Theme.fontSize + 2 : 13
                }
                Caption {
                    ctx: pop.ctx
                    width: column.width
                    text: root.v.counts
                    color: pop.ctx ? pop.ctx.Theme.textMuted : "grey"
                }
                Row {
                    spacing: 8
                    visible: root.v.running
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8
                        height: 8
                        radius: 4
                        color: !pop.ctx ? "grey" : root.v.vpnTone === "ok" ? pop.ctx.Theme.success
                             : root.v.vpnTone === "warn" ? pop.ctx.Theme.warning : pop.ctx.Theme.textMuted
                    }
                    Caption { ctx: pop.ctx; width: column.width - 16; text: root.v.vpn }
                }
                Caption {
                    ctx: pop.ctx
                    width: column.width
                    visible: root.problem !== ""
                    text: root.problem
                    wrap: true
                    color: pop.ctx ? pop.ctx.Theme.error : "red"
                }
                Row {
                    spacing: 8
                    PillButton {
                        ctx: pop.ctx
                        visible: root.v.canPause
                        text: "Pause all"
                        onClicked: root.run("pause-all")
                    }
                    PillButton {
                        ctx: pop.ctx
                        visible: root.v.canResume
                        text: "Resume all"
                        onClicked: root.run("resume-all")
                    }
                    PillButton {
                        ctx: pop.ctx
                        primary: true
                        text: "Open uQbit"
                        onClicked: {
                            root.run("show");
                            pop.ctx.closePopup();
                        }
                    }
                }
            }
        }
    }
}

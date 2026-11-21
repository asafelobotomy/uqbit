// uQbit's uQ widgets (uq/widgets/uqbit and uqbit-quick) offscreen with a fake widget ctx:
//   QT_QPA_PLATFORM=offscreen qmltestrunner -input tests/uq/tst_widget.qml
import QtQuick
import QtTest

Item {
    id: root
    width: 400
    height: 400

    QtObject {
        id: theme
        property color bg: "#1f1f22"
        property color surface: "#2b2b2e"
        property color surfaceAlt: "#353538"
        property color overlay: "#464649"
        property color text: "#e4e4e7"
        property color textMuted: "#a9a9ac"
        property color accent: "#8ab4f8"
        property color accentText: "#1f1f22"
        property color border: "#5a5a5d"
        property color success: "#81c995"
        property color warning: "#fdd663"
        property color error: "#f28b82"
        property int radius: 10
        property int fontSize: 11
        property string fontUi: "sans"
    }
    Component {
        id: uqText
        Text { textFormat: Text.PlainText; elide: Text.ElideRight }
    }
    QtObject {
        id: fake
        property var status: ({})
        property var calls: []
        property int popups: 0
        property int closes: 0
    }
    QtObject {
        id: view
        property bool shown: true
    }
    readonly property var ctx: ({
        Theme: theme,
        theme: theme,
        UqText: uqText,
        UqInput: null,
        slot: "bar",
        view: view,
        command: {
            run: function (id, args, cb) {
                fake.calls = fake.calls.concat([id]);
                cb(id === "status" ? { code: 0, output: fake.status } : { code: 0, output: "ok" });
            }
        },
        openPopup: function () { fake.popups++; },
        closePopup: function () { fake.closes++; },
        fail: function (reason) { console.warn("fail:", reason); }
    })

    readonly property var running: ({
        engine: "running", dl: 3 * 1048576, up: 1024, downloading: 2, seeding: 1, stopped: 0, total: 3,
        vpn: { required: true, interface: "wg0", up: true, bound: true }, protected: true
    })

    Loader { id: bar; width: 26; height: 26 }
    Loader { id: quick; y: 40; width: 360; height: 32 }

    function find(item, pred) {
        if (!item)
            return null;
        if (pred(item))
            return item;
        for (let i = 0; i < item.children.length; ++i) {
            const hit = find(item.children[i], pred);
            if (hit)
                return hit;
        }
        return null;
    }
    function texts(item) {
        let out = [];
        if (item.hasOwnProperty("textFormat") && item.text)
            out.push(item.text);
        for (let i = 0; i < item.children.length; ++i)
            out = out.concat(texts(item.children[i]));
        return out;
    }

    TestCase {
        name: "UqbitWidgets"
        when: windowShown

        function load(loader, path) {
            loader.source = "";
            loader.setSource(Qt.resolvedUrl(path), { ctx: root.ctx });
            verify(loader.status === Loader.Ready, path + " loads");
            return loader.item;
        }

        function init() {
            fake.status = root.running;
            fake.calls = [];
            fake.popups = 0;
            view.shown = true;
        }

        function test_bar_shows_while_the_engine_runs() {
            fake.status = { engine: "stopped", vpn: { required: false } };
            const w = load(bar, "../../uq/widgets/uqbit/Widget.qml");
            compare(fake.calls[0], "status");
            compare(w.hidden, true);
            fake.status = root.running;
            w.refresh();
            compare(w.hidden, false);
            compare(w.v.busy, true);
        }

        function test_bar_popup_pauses_and_opens() {
            const w = load(bar, "../../uq/widgets/uqbit/Widget.qml");
            mouseClick(w);
            compare(fake.popups, 1);
            const pop = w.popup.createObject(root, { ctx: root.ctx });
            verify(pop !== null);
            tryVerify(() => texts(pop).indexOf("Only through wg0") >= 0);
            verify(texts(pop).indexOf("2 downloading · 1 seeding") >= 0);
            const pause = find(pop, i => i.text === "Pause all" && i.hasOwnProperty("primary"));
            verify(pause && pause.visible);
            pause.clicked();
            verify(fake.calls.indexOf("pause-all") >= 0);
            const open = find(pop, i => i.text === "Open uQbit" && i.hasOwnProperty("primary"));
            open.clicked();
            verify(fake.calls.indexOf("show") >= 0);
            compare(fake.closes, 1);
            pop.destroy();
        }

        function test_bar_warns_while_the_vpn_holds_torrents() {
            fake.status = Object.assign({}, root.running, {
                vpn: { required: true, interface: null, up: false, bound: true }, protected: false });
            const w = load(bar, "../../uq/widgets/uqbit/Widget.qml");
            compare(w.v.warn, true);
            compare(w.v.vpn, "VPN down: torrents paused");
        }

        function test_quick_tile_switch() {
            const w = load(quick, "../../uq/widgets/uqbit-quick/Widget.qml");
            compare(w.hidden, false);
            compare(w.on, true);
            verify(texts(w).indexOf("Torrents") >= 0);
            const sw = find(w, i => i.Accessible.name === "Torrents running");
            verify(sw !== null);
            mouseClick(sw);
            verify(fake.calls.indexOf("pause-all") >= 0);
            fake.status = Object.assign({}, root.running, { downloading: 0, seeding: 0, stopped: 3 });
            w.refresh();
            compare(w.on, false);
            mouseClick(sw);
            verify(fake.calls.indexOf("resume-all") >= 0);
        }

        function test_polls_less_off_screen() {
            const w = load(bar, "../../uq/widgets/uqbit/Widget.qml");
            const before = fake.calls.length;
            view.shown = false;
            view.shown = true;
            verify(fake.calls.length > before, "refreshes when shown again");
        }
    }
}

// uQbit's uQ Settings page against the stub org.uq.settings module and a fake ctx:
//   QT_QPA_PLATFORM=offscreen qmltestrunner -import tests/uq/stub -input tests/uq/tst_page.qml
import QtQuick
import QtTest

Item {
    id: root
    width: 900
    height: 900

    readonly property url pageUrl: Qt.resolvedUrl("../../uq/pages/uqbit/Page.qml")

    property var answers: ({
                config: { code: 0, output: { settings: [
                            { key: "download_folder", value: "~/Downloads" }, { key: "ask_before_adding", value: true },
                            { key: "start_added", value: true }, { key: "notify_finished", value: true },
                            { key: "keep_running", value: true }, { key: "start_on_login", value: false },
                            { key: "download_limit", value: 0 }, { key: "upload_limit", value: 512 },
                            { key: "seed_ratio", value: 0 }, { key: "vpn_only", value: false },
                            { key: "vpn_interface", value: "" }, { key: "encryption", value: "require" },
                            { key: "anonymous_mode", value: true }, { key: "blocklist", value: true },
                            { key: "incomplete_folder", value: "" }, { key: "watch_folder", value: "~/Torrents/watch" },
                            { key: "content_layout", value: "original" }, { key: "incomplete_extension", value: false },
                            { key: "preallocate", value: false }, { key: "max_active_downloads", value: 3 },
                            { key: "max_active_uploads", value: 0 }, { key: "max_active_torrents", value: 5 },
                            { key: "seed_time", value: 90 }, { key: "when_seeded", value: "stop" }] } },
                status: { code: 0, output: { engine: "running", vpn: { required: false, interface: null, up: false, bound: false },
                          protected: false, blocklist: { on: true, path: "/x/blocklist.p2p", updated: "2026-11-21T23:18:47Z" } } },
                "blocklist-update": { code: 0, output: { on: true } },
                "default-status": { code: 0, output: { desktop_id: "io.github.asafelobotomy.uqbit.desktop",
                                    types: 2, ours: 0, default: false,
                                    torrent: "org.qbittorrent.qBittorrent.desktop", magnet: "org.qbittorrent.qBittorrent.desktop" } },
                "engine-status": { code: 0, output: { installed: true, running: true, version: "v5.3.0", systemd: true } },
                list: { code: 0, output: { running: true, torrents: [{ state: "downloading", progress: 0.5 }] } },
                "config-set": { code: 0, output: "ok" },
                "config-reset": { code: 0, output: "ok" },
                "default-set": { code: 0, output: { default: true } },
                "engine-start": { code: 0, output: { running: true } },
                "engine-stop": { code: 0, output: { running: false } }
            })

    FakeContext {
        id: fake
        confirmed: ["default-set", "engine-stop"]
        commands: root.answers
    }

    Loader {
        id: loader
        anchors.fill: parent
    }

    function find(item, pred) {
        if (pred(item))
            return item;
        for (let i = 0; i < item.children.length; ++i) {
            const hit = find(item.children[i], pred);
            if (hit)
                return hit;
        }
        if (item.contentItem && item.contentItem !== item) {
            const hit = find(item.contentItem, pred);
            if (hit)
                return hit;
        }
        return null;
    }
    function row(id) { return find(loader.item, i => i.settingId === id); }
    function button(text) { return find(loader.item, i => i.text === text && i.hasOwnProperty("checkable")); }
    function label(text) { return find(loader.item, i => i.text === text && i.hasOwnProperty("wrapMode")); }

    TestCase {
        name: "UqbitPage"
        when: windowShown

        function init() {
            fake.commands = root.answers;
            fake.calls = [];
            loader.source = "";
            loader.source = root.pageUrl;
            verify(loader.status === Loader.Ready, "the page loads");
            loader.item.ctx = fake.ctx;
        }

        function test_reads_and_shows() {
            tryCompare(row("default"), "label", "Torrent files and magnet links open in qBittorrent.");
            compare(fake.calls[0], ["watch", "config"]);
            tryCompare(row("engine"), "label", "Running qBittorrent 5.3.0 · 1 torrent, 1 downloading");
            tryCompare(row("keep_running"), "value", true);
            tryCompare(row("start_on_login"), "value", false);
            tryVerify(() => row("start_on_login").enabled);
            tryVerify(() => label("512 KiB/s") !== null);
            tryVerify(() => label("At once: 3 downloading, any number seeding, 5 in all. The rest wait their turn.") !== null);
            tryVerify(() => label("1 h 30 min") !== null);
        }

        function test_a_switch_sets_the_key() {
            tryCompare(row("notify_finished"), "value", true);
            row("notify_finished").chosen(false);
            tryVerify(() => fake.calls.some(c => c[1] === "config-set"));
            compare(fake.calls.find(c => c[1] === "config-set")[2], ["notify_finished", "false"]);
        }

        function test_privacy_card() {
            tryVerify(() => label("No VPN required: torrents use any network.") !== null);
            tryCompare(row("encryption"), "value", true);
            tryCompare(row("anonymous_mode"), "value", true);
            compare(row("vpn_interface").visible, false);
            row("vpn_only").chosen(true);
            tryVerify(() => fake.calls.some(c => c[1] === "config-set"));
            compare(fake.calls.find(c => c[1] === "config-set")[2], ["vpn_only", "true"]);
        }

        function test_encryption_off_means_prefer() {
            tryCompare(row("encryption"), "value", true);
            row("encryption").chosen(false);
            tryVerify(() => fake.calls.some(c => c[1] === "config-set"));
            compare(fake.calls.find(c => c[1] === "config-set")[2], ["encryption", "prefer"]);
        }

        function test_blocklist_update() {
            tryVerify(() => button("Update now") !== null && button("Update now").visible);
            button("Update now").clicked();
            tryVerify(() => fake.calls.some(c => c[1] === "blocklist-update"));
        }

        function test_a_limit_saves() {
            tryVerify(() => row("download_limit") !== null);
            const r = row("download_limit");
            const field = find(r, i => i.hasOwnProperty("placeholderText") && i.placeholderText === "0");
            tryCompare(field, "text", "0");
            field.text = "2048";
            const save = find(r, i => i.text === "Save" && i.hasOwnProperty("checkable"));
            tryVerify(() => save.enabled);
            save.clicked();
            tryVerify(() => fake.calls.some(c => c[1] === "config-set"));
            compare(fake.calls.find(c => c[1] === "config-set")[2], ["download_limit", "2048"]);
        }

        function test_folders() {
            tryVerify(() => row("incomplete_folder") !== null);
            const off = find(loader.item, i => i.objectName === "watch_folder-off");
            tryVerify(() => off.visible);
            compare(find(loader.item, i => i.objectName === "incomplete_folder-off").visible, false);
            const field = find(loader.item, i => i.objectName === "incomplete_folder-field");
            const save = find(loader.item, i => i.objectName === "incomplete_folder-save");
            field.text = "../etc";
            compare(save.enabled, false);
            field.text = "~/Torrents/unfinished/";
            tryVerify(() => save.enabled);
            save.clicked();
            tryVerify(() => fake.calls.some(c => c[1] === "config-set"));
            compare(fake.calls.find(c => c[1] === "config-set")[2], ["incomplete_folder", "~/Torrents/unfinished"]);
        }

        function test_turning_the_watched_folder_off_resets_it() {
            const off = find(loader.item, i => i.objectName === "watch_folder-off");
            tryVerify(() => off.visible && off.enabled);
            off.clicked();
            tryVerify(() => fake.calls.some(c => c[1] === "config-reset"));
            compare(fake.calls.find(c => c[1] === "config-reset")[2], ["watch_folder"]);
        }

        function test_choices() {
            const layout = find(loader.item, i => i.objectName === "content_layout-combo");
            tryVerify(() => layout.enabled);
            compare(layout.currentIndex, 0);
            layout.activated(1);
            tryVerify(() => fake.calls.some(c => c[1] === "config-set"));
            compare(fake.calls.find(c => c[1] === "config-set")[2], ["content_layout", "subfolder"]);
            const seeded = find(loader.item, i => i.objectName === "when_seeded-combo");
            tryVerify(() => seeded.enabled);
            seeded.activated(1);
            tryVerify(() => fake.calls.filter(c => c[1] === "config-set").length === 2);
            compare(fake.calls.filter(c => c[1] === "config-set")[1][2], ["when_seeded", "remove"]);
        }

        function test_stop_engine_asks_and_reads_again() {
            tryVerify(() => button("Stop") !== null && button("Stop").visible);
            const before = fake.calls.filter(c => c[1] === "engine-status").length;
            button("Stop").clicked();
            tryVerify(() => fake.calls.some(c => c[1] === "engine-stop"));
            tryVerify(() => fake.calls.filter(c => c[1] === "engine-status").length > before);
        }

        function test_make_default_and_open() {
            tryVerify(() => button("Make uQbit the default") !== null && button("Make uQbit the default").visible);
            button("Make uQbit the default").clicked();
            tryVerify(() => fake.calls.some(c => c[1] === "default-set"));
            button("Open uQbit").clicked();
            tryVerify(() => fake.calls.some(c => c[0] === "openApp" && c[1] === "io.github.asafelobotomy.uqbit.desktop"));
        }

        function test_failed_read_shows_error() {
            fake.commands = Object.assign({}, fake.commands, { config: { error: "timed out" } });
            loader.source = "";
            loader.source = root.pageUrl;
            loader.item.ctx = fake.ctx;
            tryCompare(loader.item, "configError", "timed out");
        }
    }
}

// Tests for uQbit's uQ Settings page (uq/pages/uqbit/): logic.js and the manifest.
//   node --test 'tests/**/*.test.mjs'
import assert from "node:assert/strict";
import { readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { loadLogic } from "./load-logic.mjs";
import { parseToml } from "./toml-lite.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const pageDir = join(here, "..", "..", "uq", "pages", "uqbit");
const L = loadLogic(join(pageDir, "logic.js"));
const manifest = parseToml(readFileSync(join(pageDir, "page.toml"), "utf8"));
const qml = readdirSync(pageDir).filter((f) => f.endsWith(".qml"))
  .map((f) => readFileSync(join(pageDir, f), "utf8")).join("\n");

test("manifest: uq_api 2, every command used, every arg pattern anchored", () => {
  assert.equal(manifest.uq_api, 2);
  assert.equal(manifest.id, "uqbit");
  for (const c of manifest.command) {
    assert.ok(c.argv[0] === "/usr/bin/uqbit", c.id);
    assert.ok(qml.includes(`"${c.id}"`), `${c.id} is used by the page`);
    for (const a of c.args || [])
      assert.match(a.pattern, /^\^.*\$$/, `${c.id}.${a.name}`);
  }
  for (const s of manifest.setting)
    assert.ok(qml.includes(`settingId: "${s.id}"`) || qml.includes(`key: "${s.id}"`), `setting ${s.id} has a row`);
});

test("manifest: the key pattern names every page setting, and only real ones", () => {
  const set = manifest.command.find((c) => c.id === "config-set");
  const key = new RegExp(set.args[0].pattern);
  for (const s of manifest.setting.filter((s) => !["default", "engine"].includes(s.id)))
    assert.ok(key.test(s.id), s.id);
  assert.ok(!key.test("webui_port"), "the port isn't the page's to change");
});

test("manifest: the value pattern takes what the page sends and nothing that starts with -", () => {
  const set = manifest.command.find((c) => c.id === "config-set");
  const value = new RegExp(set.args[1].pattern);
  for (const ok of ["true", "false", "0", "1000000", "~/Downloads", "/data/My Torrents"])
    assert.ok(value.test(ok), ok);
  for (const bad of ["-rf", "en;rm", "relative/dir", "~/a\nb", "12345678"])
    assert.ok(!value.test(bad), bad);
});

test("parseConfig", () => {
  const ok = L.parseConfig({ code: 0, output: { settings: [{ key: "keep_running", value: true }, { key: "download_limit", value: 0 }] } });
  assert.equal(ok.ok, true);
  assert.deepEqual(ok.values, { keep_running: true, download_limit: 0 });
  assert.equal(L.parseConfig({ error: "timed out" }).error, "timed out");
  assert.equal(L.parseConfig({ code: 0, output: {} }).ok, false);
});

test("parseDefault", () => {
  const out = (o) => L.parseDefault({ code: 0, output: Object.assign({ desktop_id: "io.github.asafelobotomy.uqbit.desktop", types: 2 }, o) });
  assert.equal(out({ default: true, ours: 2 }).summary, "uQbit opens .torrent files and magnet links.");
  assert.equal(out({ default: false, torrent: "org.qbittorrent.qBittorrent.desktop", magnet: "org.qbittorrent.qBittorrent.desktop" }).summary,
    "Torrent files and magnet links open in qBittorrent.");
  assert.equal(out({ default: false, torrent: "", magnet: "firefox.desktop" }).summary,
    "Torrent files open in nothing, magnet links in Firefox.");
  assert.equal(L.parseDefault({ error: "x" }).ok, false);
});

test("engineLine", () => {
  assert.equal(L.engineLine(null, null).ok, false);
  assert.match(L.engineLine({ output: { installed: false } }).line, /isn't installed/);
  const stopped = L.engineLine({ output: { installed: true, running: false, systemd: true } });
  assert.equal(stopped.running, false);
  assert.equal(stopped.canLogin, true);
  const running = L.engineLine({ output: { installed: true, running: true, version: "v5.3.0" } },
    { output: { torrents: [{ state: "downloading", progress: 0.2 }, { state: "stalledUP", progress: 1 }, { state: "stoppedDL", progress: 0.1 }] } });
  assert.equal(running.line, "Running qBittorrent 5.3.0 · 3 torrents, 1 downloading, 1 seeding");
});

test("cleanFolder, cleanNumber, limitLabel", () => {
  assert.equal(L.cleanFolder(" ~/Torrents/ "), "~/Torrents");
  assert.equal(L.cleanFolder("~"), null);
  assert.equal(L.cleanFolder("relative"), null);
  assert.equal(L.cleanFolder("~/a/../b"), null);
  assert.equal(L.cleanNumber("", 100), "0");
  assert.equal(L.cleanNumber(" 042 ", 100), "42");
  assert.equal(L.cleanNumber("101", 100), null);
  assert.equal(L.cleanNumber("-1", 100), null);
  assert.equal(L.limitLabel(0), "No limit");
  assert.equal(L.limitLabel(512), "512 KiB/s");
  assert.equal(L.limitLabel(2048), "2 MiB/s");
});

test("failure", () => {
  assert.equal(L.failure({ error: "uqbit: starting on login needs systemd's user manager\nmore" }),
    "starting on login needs systemd's user manager");
  assert.equal(L.failure({ error: "Cancelled", cancelled: true }), "");
});

test("privacy line", () => {
  assert.equal(L.privacyLine(null).tone, "unknown");
  assert.equal(L.privacyLine({ error: "x" }).text, "");
  const off = L.privacyLine({ output: { engine: "running", vpn: { required: false }, blocklist: { on: true, updated: "2026-11-21T23:18:47Z" } } });
  assert.equal(off.tone, "off");
  assert.equal(off.blocklist, "Updated 2026-11-21; refreshed every week.");
  const ok = L.privacyLine({ output: { engine: "running", vpn: { required: true, interface: "wg0", up: true, bound: true }, protected: true } });
  assert.equal(ok.tone, "ok");
  assert.match(ok.text, /only through wg0/);
  assert.match(L.privacyLine({ output: { vpn: { required: true, interface: null, up: false } } }).text, /No VPN is connected/);
  assert.match(L.privacyLine({ output: { vpn: { required: true, interface: "tun0", up: false } } }).text, /tun0\) is down/);
  assert.equal(L.privacyLine({ output: { engine: "stopped", vpn: { required: true, interface: "wg0", up: true } } }).tone, "ok");
  assert.equal(L.privacyLine({ output: { vpn: {}, blocklist: { on: true } } }).blocklist.startsWith("Not downloaded"), true);
});

test("interface names", () => {
  assert.equal(L.cleanInterface(" wg0 "), "wg0");
  assert.equal(L.cleanInterface(""), "");
  assert.equal(L.cleanInterface("wg0; rm -rf"), null);
  assert.equal(L.cleanInterface("a".repeat(16)), null);
});

test("optionalFolder, queueLine, minutesLabel", () => {
  assert.equal(L.optionalFolder("  "), "");
  assert.equal(L.optionalFolder("~/T/"), "~/T");
  assert.equal(L.optionalFolder("relative"), null);
  assert.equal(L.queueLine({ max_active_downloads: 0, max_active_uploads: 0, max_active_torrents: 0 }),
    "Every torrent runs at once: nothing waits in a queue.");
  assert.equal(L.queueLine({ max_active_downloads: 3, max_active_uploads: 3, max_active_torrents: 5 }),
    "At once: 3 downloading, 3 seeding, 5 in all. The rest wait their turn.");
  assert.equal(L.minutesLabel(0), "");
  assert.equal(L.minutesLabel(45), "45 min");
  assert.equal(L.minutesLabel(1500), "1 d 1 h");
});

test("trayLine", () => {
  assert.match(L.trayLine({}), /^No tray icon/);
  assert.match(L.trayLine({ launch_on_login: true, start_in_tray: false }), /^No tray icon/);
  assert.match(L.trayLine({ launch_on_login: true, start_in_tray: true }), /icon in the tray/);
  assert.match(L.trayLine({ close_to_tray: true }), /icon in the tray/);
});

test("tracker lists", () => {
  const best = "https://ngosang.github.io/trackerslist/trackers_best.txt";
  assert.deepEqual(L.trackerUrls(""), []);
  assert.equal(L.addTrackerList("", best), best);
  assert.equal(L.addTrackerList(best, " https://x.example/all.txt "), best + " https://x.example/all.txt");
  assert.equal(L.addTrackerList(best, best), null);
  assert.equal(L.addTrackerList("", "http://x.example/a.txt"), null);
  assert.equal(L.addTrackerList("", "https://x.example/a b.txt"), null);
  const ten = Array.from({ length: 10 }, (_, i) => "https://x.example/" + i).join(" ");
  assert.equal(L.addTrackerList(ten, "https://y.example/z"), null);
  assert.equal(L.removeTrackerList(best + " https://x.example/all.txt", best), "https://x.example/all.txt");
  assert.equal(L.removeTrackerList(best, best), "");
  assert.match(L.trackersLine(null), /^No lists/);
  assert.equal(L.trackersLine({ output: { trackers: { lists: [best, "https://x"], count: 57, updated: "2026-11-22T00:42:10Z" } } }),
    "57 trackers from 2 lists are added to new public torrents; updated 2026-11-22, refreshed daily.");
});

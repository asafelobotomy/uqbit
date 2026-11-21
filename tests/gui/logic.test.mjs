// Tests for the window's pure helpers (qml/logic.js).
//   node --test 'tests/**/*.test.mjs'
import assert from "node:assert/strict";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { loadLogic } from "../uq/load-logic.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const L = loadLogic(join(here, "..", "..", "qml", "logic.js"));

test("bytes and speed", () => {
  assert.equal(L.bytes(0), "0 B");
  assert.equal(L.bytes(1023), "1023 B");
  assert.equal(L.bytes(1536), "1.50 KB");
  assert.equal(L.bytes(180 * 1024 * 1024), "180 MB");
  assert.equal(L.bytes(40 * 1024 * 1024), "40.0 MB");
  assert.equal(L.bytes(3 * 1024 ** 4), "3.00 TB");
  assert.equal(L.bytes("x"), "0 B");
  assert.equal(L.speed(0), "");
  assert.equal(L.speed(263 * 1024), "263 KB/s");
});

test("eta", () => {
  assert.equal(L.eta(8640000), "");
  assert.equal(L.eta(-1), "");
  assert.equal(L.eta(0.2), "1 s");
  assert.equal(L.eta(45), "45 s");
  assert.equal(L.eta(600), "10 min");
  assert.equal(L.eta(3600), "1 h");
  assert.equal(L.eta(7500), "2 h 5 min");
  assert.equal(L.eta(86400 * 3), "3 days");
});

test("percent and ratio", () => {
  assert.equal(L.percent(0), "0.0%");
  assert.equal(L.percent(0.4567), "45.6%");
  assert.equal(L.percent(0.9999), "99.9%");
  assert.equal(L.percent(1), "100%");
  assert.equal(L.ratio(1.234), "1.23");
  assert.equal(L.ratio(-1), "∞");
  assert.equal(L.ratio(1e9), "∞");
});

test("state labels and tones", () => {
  assert.equal(L.stateLabel("stalledDL", 0.3), "Waiting for peers");
  assert.equal(L.stateLabel("stoppedUP", 1), "Finished");
  assert.equal(L.stateLabel("pausedDL", 0.2), "Stopped");
  assert.equal(L.stateLabel("metaDL", 0), "Getting info");
  assert.equal(L.stateLabel("weird", 1), "Finished");
  assert.equal(L.stateTone("error", "error"), "error");
  assert.equal(L.stateTone("seeding", "uploading"), "success");
  assert.equal(L.stateTone("downloading", "stalledDL"), "warning");
  assert.equal(L.stateTone("downloading", "downloading"), "accent");
  assert.equal(L.isRunning("stopped"), false);
  assert.equal(L.isRunning("seeding"), true);
});

test("summary", () => {
  const mb = 1024 * 1024;
  assert.equal(L.summary({ progress: 0.5, size: 100 * mb, downloaded: 50 * mb, dlspeed: mb, upspeed: 0, eta: 50 }),
    "50.0 MB of 100 MB  ·  ↓ 1.00 MB/s  ·  50 s left");
  assert.equal(L.summary({ progress: 1, size: 40 * mb, downloaded: 40 * mb, dlspeed: 0, upspeed: 2048, ratio: 0.5 }),
    "40.0 MB  ·  ↑ 2.00 KB/s  ·  ratio 0.50");
  assert.equal(L.summary({ progress: 0, size: 0 }), "Getting its details from peers…");
});

test("torrentInputs picks magnets, torrent URLs and files", () => {
  assert.deepEqual(L.torrentInputs([
    "magnet:?xt=urn:btih:abc&dn=x\nhttps://example.org/a.torrent\nhello",
    "file:///home/me/x.torrent", "file:///home/me/x.iso", "https://example.org/page"
  ]), ["magnet:?xt=urn:btih:abc&dn=x", "https://example.org/a.torrent", "file:///home/me/x.torrent"]);
  assert.deepEqual(L.torrentInputs([]), []);
  assert.deepEqual(L.torrentInputs(undefined), []);
});

test("filters and their empty text", () => {
  assert.deepEqual(L.filters.map((f) => f.id), ["all", "downloading", "seeding", "finished", "stopped", "error"]);
  for (const f of L.filters)
    assert.notEqual(L.emptyText(f.id, false), "", f.id);
  assert.match(L.emptyText("all", true), /name/);
});

test("titles, dates, names, trackers", () => {
  assert.equal(L.windowTitle(0, 0), "uQbit");
  assert.equal(L.windowTitle(2048, 0), "uQbit — ↓ 2.00 KB/s");
  assert.equal(L.date(0), "");
  assert.match(L.date(1795300000), /^\d{1,2} [A-Z][a-z]{2} 20\d\d, \d\d:\d\d$/);
  assert.equal(L.baseName("a/b/c.mkv"), "c.mkv");
  assert.equal(L.baseName("c.mkv"), "c.mkv");
  assert.equal(L.trackerStatus(2), "Working");
  assert.equal(L.trackerStatus(9), "");
});

test("privacy status in words", () => {
  assert.equal(L.protection({}).level, "off");
  assert.equal(L.protection({ required: false, protected: false }).title, "No VPN required");
  const ok = L.protection({ required: true, protected: true, up: true, bound: true, interface: "wg0" });
  assert.equal(ok.level, "ok");
  assert.match(ok.detail, /wg0/);
  const down = L.protection({ required: true, protected: false, up: false, bound: true, interface: "wg0" });
  assert.equal(down.level, "warn");
  assert.match(down.title, /paused/);
  assert.match(L.protection({ required: true, up: false, interface: null }).detail, /a VPN connects/);
  assert.match(L.protection({ required: true, up: true, bound: false, interface: "tun0" }).title, /Binding/);
});

test("search result lines and categories", () => {
  assert.equal(L.resultLine({ size: 1536, seeds: 12, leechers: 1, site: "example.org" }),
    "1.50 KB · 12 seeds · 1 peer · example.org");
  assert.equal(L.resultLine({ size: -1, seeds: 1, leechers: -1, site: "" }), "1 seed");
  assert.equal(L.categories[0].id, "all");
  assert.ok(L.categories.some((c) => c.id === "tv"));
});

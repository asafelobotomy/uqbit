// Tests for the uQ widgets' helpers (uq/widgets/uqbit/logic.js; the quick tile has a copy).
//   node --test 'tests/**/*.test.mjs'
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { loadLogic } from "./load-logic.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const bar = join(root, "uq", "widgets", "uqbit");
const quick = join(root, "uq", "widgets", "uqbit-quick");
const L = loadLogic(join(bar, "logic.js"));

test("the quick tile's copies match the bar widget's", () => {
  for (const f of ["logic.js", "Caption.qml"])
    assert.equal(readFileSync(join(quick, f), "utf8"), readFileSync(join(bar, f), "utf8"), f);
});

test("speeds", () => {
  assert.equal(L.speed(0), "0 B/s");
  assert.equal(L.speed(1536), "1.5 KB/s");
  assert.equal(L.speed(250 * 1048576), "250 MB/s");
});

test("what the widget shows", () => {
  const stopped = L.view({ engine: "stopped", vpn: { required: false } });
  assert.equal(stopped.running, false);
  assert.match(stopped.counts, /stopped/);
  assert.match(L.view({ engine: "missing" }).counts, /isn't installed/);
  assert.equal(L.view(undefined).running, false);

  const busy = L.view({ engine: "running", dl: 2048, up: 0, downloading: 2, seeding: 3, stopped: 1,
                        vpn: { required: true, interface: "wg0", up: true, bound: true }, protected: true });
  assert.equal(busy.busy, true);
  assert.equal(busy.warn, false);
  assert.equal(busy.counts, "2 downloading · 3 seeding · 1 stopped");
  assert.equal(busy.vpn, "Only through wg0");
  assert.equal(busy.vpnTone, "ok");
  assert.equal(busy.canPause, true);
  assert.equal(busy.canResume, true);
  assert.equal(busy.speeds, "↓ 2.0 KB/s   ↑ 0 B/s");

  const held = L.view({ engine: "running", downloading: 1, vpn: { required: true, interface: null, up: false, bound: true }, protected: false });
  assert.equal(held.warn, true);
  assert.equal(held.vpn, "VPN down: torrents paused");
  assert.equal(held.vpnTone, "warn");

  const idle = L.view({ engine: "running", vpn: { required: false } });
  assert.equal(idle.counts, "No torrents");
  assert.equal(idle.vpnTone, "off");
  assert.equal(idle.canPause || idle.canResume, false);
});

test("manifests: every command is a uqbit subcommand the widget uses", () => {
  for (const dir of [bar, quick]) {
    const toml = readFileSync(join(dir, "widget.toml"), "utf8");
    const qml = readFileSync(join(dir, "Widget.qml"), "utf8");
    const ids = [...toml.matchAll(/^id = "([^"]+)"/gm)].map((m) => m[1]);
    assert.equal(ids[0], dir.endsWith("quick") ? "uqbit-quick" : "uqbit");
    for (const id of ids.slice(1))
      assert.ok(qml.includes(`"${id}"`), `${dir}: ${id} is used`);
    for (const m of toml.matchAll(/^argv = \[(.*)\]$/gm))
      assert.match(m[1], /^"\/usr\/bin\/uqbit", /);
    // Widgets draw text only through ctx.UqText.
    assert.doesNotMatch(qml, /^\s*(Text|Label|TextField)\s*\{/m);
  }
});

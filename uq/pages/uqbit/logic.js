.pragma library
// Pure helpers for uQbit's uQ Settings page: no Qt calls, so tests/uq/logic.test.mjs runs them
// with Node.

/// `uqbit config show --json` -> {ok, values: {key: value}, settings: [...]}, or {ok:false, error}.
function parseConfig(result) {
    if (!result || result.error)
        return { ok: false, error: (result && result.error) || "No answer from uqbit" };
    const out = result.output;
    if (!out || !Array.isArray(out.settings))
        return { ok: false, error: "uqbit gave an answer this page doesn't understand" };
    const values = {};
    for (const s of out.settings)
        if (s && typeof s.key === "string")
            values[s.key] = s.value;
    return { ok: true, values: values, settings: out.settings };
}

/// "qbittorrent.desktop" -> "qBittorrent", "org.qbittorrent.qBittorrent.desktop" -> "qBittorrent".
function prettyApp(id) {
    let s = String(id || "").replace(/\.desktop$/, "");
    const known = { qbittorrent: "qBittorrent", "org.qbittorrent.qBittorrent": "qBittorrent",
                    "transmission-gtk": "Transmission", "transmission-qt": "Transmission",
                    "com.transmissionbt.Transmission": "Transmission", "org.kde.ktorrent": "KTorrent",
                    deluge: "Deluge", "org.deluge_torrent.deluge": "Deluge", firefox: "Firefox",
                    "de.haeckerfelix.Fragments": "Fragments" };
    if (known[s])
        return known[s];
    const parts = s.split(".");
    return parts[parts.length - 1] || s;
}

/// `uqbit default status --json` -> {ok, isDefault, summary}.
function parseDefault(result) {
    if (!result || result.error || !result.output)
        return { ok: false, error: (result && result.error) || "No answer from uqbit" };
    const o = result.output;
    const name = id => !id ? "nothing" : id === o.desktop_id ? "uQbit" : prettyApp(id);
    let summary;
    if (o.default)
        summary = "uQbit opens .torrent files and magnet links.";
    else if (o.torrent === o.magnet)
        summary = "Torrent files and magnet links open in " + name(o.torrent) + ".";
    else
        summary = "Torrent files open in " + name(o.torrent) + ", magnet links in " + name(o.magnet) + ".";
    return { ok: true, isDefault: o.default === true, summary: summary };
}

/// `uqbit engine status --json` (+ `uqbit list --json`) -> {ok, installed, running, line, canLogin}.
function engineLine(status, list) {
    if (!status || status.error || !status.output)
        return { ok: false, installed: false, running: false, canLogin: false,
                 line: (status && status.error) || "Checking…" };
    const s = status.output;
    if (!s.installed)
        return { ok: true, installed: false, running: false, canLogin: false,
                 line: "qbittorrent-nox isn't installed. Install it (or qbittorrent-enhanced-nox) to use uQbit." };
    if (!s.running)
        return { ok: true, installed: true, running: false, canLogin: s.systemd === true,
                 line: "Stopped. It starts when you open uQbit." };
    let line = "Running qBittorrent " + String(s.version || "").replace(/^v/, "");
    const t = list && list.output && Array.isArray(list.output.torrents) ? list.output.torrents : null;
    if (t) {
        const dl = t.filter(x => x.progress < 1 && !/^(stopped|paused|error|missing)/.test(x.state)).length;
        const seed = t.filter(x => /UP$|^uploading$/.test(x.state) && !/^(stopped|paused)/.test(x.state)).length;
        const bits = [t.length === 1 ? "1 torrent" : t.length + " torrents"];
        if (dl)
            bits.push(dl + " downloading");
        if (seed)
            bits.push(seed + " seeding");
        line += " · " + bits.join(", ");
    }
    return { ok: true, installed: true, running: true, canLogin: s.systemd === true, line: line };
}

/// The argument `uqbit config set` gets for a switch.
function boolArg(on) {
    return on ? "true" : "false";
}

/// A typed folder: "~/Torrents" or "/data/torrents"; null when it isn't one the page may send.
function cleanFolder(text) {
    const t = String(text || "").trim().replace(/\/+$/, "");
    if (t === "~" || t === "")
        return null;
    return /^~?\/[A-Za-z0-9 ._+@'()\/-]{0,300}$/.test(t) && !/(^|\/)\.\.(\/|$)/.test(t) ? t : null;
}

/// A typed whole number from 0 to max ("" = 0); null when it isn't one.
function cleanNumber(text, max) {
    const t = String(text === undefined || text === null ? "" : text).trim();
    if (t === "")
        return "0";
    if (!/^[0-9]{1,7}$/.test(t))
        return null;
    const n = parseInt(t, 10);
    return n <= max ? String(n) : null;
}

/// A limit for the row under it: 0 -> "No limit", 2048 -> "2 MiB/s".
function limitLabel(kib) {
    const n = Number(kib) || 0;
    if (n <= 0)
        return "No limit";
    if (n >= 1024)
        return (Math.round(n / 102.4) / 10) + " MiB/s";
    return n + " KiB/s";
}

/// The first line of what a failed change said, for the row under it.
function failure(result) {
    if (!result)
        return "No answer from uqbit";
    if (result.cancelled)
        return "";
    const e = String(result.error || "");
    const m = e.match(/uqbit: (.*)/);
    return (m ? m[1] : e).split("\n")[0];
}

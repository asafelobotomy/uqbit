.pragma library
// Pure helpers for uQbit's window: no Qt calls, so tests/gui/logic.test.mjs runs them with Node.

/// 1536 -> "1.5 KB", 0 -> "0 B". Powers of 1024, labelled the way file managers do.
function bytes(n) {
    n = Number(n) || 0;
    if (n < 1024)
        return Math.max(0, Math.round(n)) + " B";
    const units = ["KB", "MB", "GB", "TB", "PB"];
    let i = -1;
    do {
        n /= 1024;
        ++i;
    } while (n >= 1024 && i < units.length - 1);
    return (n >= 100 ? n.toFixed(0) : n >= 10 ? n.toFixed(1) : n.toFixed(2)) + " " + units[i];
}

/// Bytes per second -> "3.4 MB/s"; "" for nothing.
function speed(n) {
    n = Number(n) || 0;
    return n <= 0 ? "" : bytes(n) + "/s";
}

/// Seconds left -> "2 h 5 min", "45 s"; "" when unknown (qBittorrent says 8640000).
function eta(s) {
    s = Number(s);
    if (!isFinite(s) || s < 0 || s >= 8640000)
        return "";
    if (s < 60)
        return Math.max(1, Math.round(s)) + " s";
    const m = Math.round(s / 60);
    if (m < 60)
        return m + " min";
    const h = Math.floor(m / 60);
    if (h < 48)
        return h + " h" + (m % 60 ? " " + (m % 60) + " min" : "");
    return Math.round(h / 24) + " days";
}

/// 0.4567 -> "45.7%", 1 -> "100%".
function percent(p) {
    p = Math.max(0, Math.min(1, Number(p) || 0));
    return p >= 1 ? "100%" : (Math.floor(p * 1000) / 10).toFixed(1) + "%";
}

/// A share ratio: 1.234 -> "1.23"; qBittorrent's -1 / huge values -> "∞".
function ratio(r) {
    r = Number(r);
    if (!isFinite(r) || r < 0 || r > 9999)
        return "∞";
    return r.toFixed(2);
}

/// What a qBittorrent state means, in words.
function stateLabel(state, progress) {
    const words = {
        error: "Error", missingFiles: "Files missing", uploading: "Seeding", forcedUP: "Seeding",
        stalledUP: "Seeding", queuedUP: "Queued", checkingUP: "Checking", stoppedUP: "Finished",
        pausedUP: "Finished", allocating: "Allocating", downloading: "Downloading",
        forcedDL: "Downloading", metaDL: "Getting info", forcedMetaDL: "Getting info",
        stalledDL: "Waiting for peers", queuedDL: "Queued", checkingDL: "Checking",
        checkingResumeData: "Checking", stoppedDL: "Stopped", pausedDL: "Stopped", moving: "Moving"
    };
    if (words[state])
        return words[state];
    return Number(progress) >= 1 ? "Finished" : "Unknown";
}

/// The theme colour key a state shows in: accent, success, warning, error or muted.
function stateTone(group, state) {
    if (group === "error")
        return "error";
    if (group === "seeding")
        return "success";
    if (group === "stopped")
        return "muted";
    if (state === "stalledDL" || state === "metaDL" || group === "checking")
        return "warning";
    return "accent";
}

/// Whether a torrent is running (so its button stops it).
function isRunning(group) {
    return group !== "stopped" && group !== "error";
}

/// The line under a torrent's name.
function summary(t) {
    const done = Number(t.progress) >= 1;
    const parts = [];
    if (!done && !(Number(t.size) > 0))
        return "Getting its details from peers…";
    if (done)
        parts.push(bytes(t.size));
    else
        parts.push(bytes(t.downloaded > t.size ? t.size : t.downloaded) + " of " + bytes(t.size));
    if (t.dlspeed > 0)
        parts.push("↓ " + speed(t.dlspeed));
    if (t.upspeed > 0)
        parts.push("↑ " + speed(t.upspeed));
    if (!done && t.dlspeed > 0 && eta(t.eta) !== "")
        parts.push(eta(t.eta) + " left");
    if (done)
        parts.push("ratio " + ratio(t.ratio));
    return parts.join("  ·  ");
}

/// The filters down the side, in order.
var filters = [
    { id: "all", label: "All", icon: "all" },
    { id: "downloading", label: "Downloading", icon: "down" },
    { id: "seeding", label: "Seeding", icon: "seed" },
    { id: "finished", label: "Finished", icon: "check" },
    { id: "stopped", label: "Stopped", icon: "stop" },
    { id: "error", label: "Errors", icon: "error" }
];

/// What a filter with nothing in it says.
function emptyText(filter, searching) {
    if (searching)
        return "No torrent's name has that in it.";
    const t = {
        all: "Open a .torrent file or paste a magnet link. You can drop them here too.",
        downloading: "Nothing is downloading.", seeding: "Nothing is seeding.",
        finished: "Nothing has finished yet.", stopped: "Nothing is stopped.",
        error: "No torrent has a problem."
    };
    return t[filter] || "";
}

/// Dropped or chosen things (URLs, file:// URLs or text, possibly several lines) -> the
/// magnet links, torrent URLs and .torrent files among them.
function torrentInputs(list) {
    const out = [];
    for (const raw of list || []) {
        for (const line of String(raw).split(/\r?\n/)) {
            const s = line.trim();
            if (/^magnet:\?/i.test(s))
                out.push(s);
            else if (/^https?:\/\/\S+\.torrent(\?\S*)?$/i.test(s))
                out.push(s);
            else if (/^file:\/\/\S+\.torrent$/i.test(s))
                out.push(s);
        }
    }
    return out;
}

/// Unix seconds -> "21 Nov 2026, 14:05"; "" for none.
function date(secs) {
    secs = Number(secs);
    if (!isFinite(secs) || secs <= 0)
        return "";
    const d = new Date(secs * 1000);
    const months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
    const pad = n => (n < 10 ? "0" : "") + n;
    return d.getDate() + " " + months[d.getMonth()] + " " + d.getFullYear() + ", "
        + pad(d.getHours()) + ":" + pad(d.getMinutes());
}

/// A file's name without its folders, for the files list.
function baseName(path) {
    const s = String(path || "");
    const i = s.lastIndexOf("/");
    return i < 0 ? s : s.slice(i + 1);
}

/// The window title: "uQbit" or "uQbit — ↓ 3.4 MB/s".
function windowTitle(down, up) {
    const parts = [];
    if (down > 0)
        parts.push("↓ " + speed(down));
    if (up > 0)
        parts.push("↑ " + speed(up));
    return parts.length ? "uQbit — " + parts.join("  ") : "uQbit";
}

/// Tracker status (torrents/trackers) -> words.
function trackerStatus(s) {
    return ["Disabled", "Not contacted", "Working", "Updating", "Not working"][Number(s)] || "";
}

/// What the window says about privacy, from App.protection: {level: "ok"|"warn"|"off",
/// icon, title, detail}.
function protection(p) {
    p = p || {};
    if (!p.required)
        return { level: "off", icon: "shield-off", title: "No VPN required",
                 detail: "Torrents use any network. Turn on “Only through the VPN” to stop them whenever the VPN is down." };
    if (p.protected)
        return { level: "ok", icon: "shield", title: "Protected by the VPN",
                 detail: "Torrents go only through " + p.interface + ". If it drops, they stop." };
    if (!p.up)
        return { level: "warn", icon: "shield-off", title: "VPN down: torrents paused",
                 detail: p.interface ? "Nothing is sent until " + p.interface + " is back."
                                     : "Nothing is sent until a VPN connects." };
    return { level: "warn", icon: "shield-off", title: "Binding to the VPN…",
             detail: "The engine is moving its traffic to " + p.interface + "." };
}

/// A search result's line under its name: "1.4 GB · 120 seeds · 8 peers · example.org".
function resultLine(r) {
    const parts = [];
    if (Number(r.size) > 0)
        parts.push(bytes(r.size));
    parts.push((Number(r.seeds) >= 0 ? r.seeds : "?") + (Number(r.seeds) === 1 ? " seed" : " seeds"));
    if (Number(r.leechers) >= 0)
        parts.push(r.leechers + (Number(r.leechers) === 1 ? " peer" : " peers"));
    if (r.site)
        parts.push(r.site);
    return parts.join(" · ");
}

/// The search categories qBittorrent's plugins know.
var categories = [
    { id: "all", label: "Everything" }, { id: "movies", label: "Films" }, { id: "tv", label: "TV" },
    { id: "music", label: "Music" }, { id: "games", label: "Games" }, { id: "anime", label: "Anime" },
    { id: "software", label: "Software" }, { id: "books", label: "Books" }, { id: "pictures", label: "Pictures" },
];

.pragma library
// Pure helpers for uQbit's uQ widgets (bar and quick settings): no Qt calls, so
// tests/uq/widget.test.mjs runs them with Node. The quick tile's copy must stay identical.

/// Bytes per second -> "3.4 MB/s"; "0 B/s" for nothing.
function speed(n) {
    n = Number(n) || 0;
    if (n < 1024)
        return Math.max(0, Math.round(n)) + " B/s";
    var units = ["KB/s", "MB/s", "GB/s"];
    var i = -1;
    do {
        n /= 1024;
        ++i;
    } while (n >= 1024 && i < units.length - 1);
    return (n >= 100 ? n.toFixed(0) : n.toFixed(1)) + " " + units[i];
}

/// `uqbit status --json` -> what the widget shows: {running, busy, warn, speeds, counts, vpn,
/// vpnTone ("ok"|"warn"|"off"), canResume, canPause}.
function view(s) {
    s = s || {};
    var running = s.engine === "running";
    var vpn = s.vpn || {};
    var downloading = Number(s.downloading) || 0, seeding = Number(s.seeding) || 0;
    var stopped = Number(s.stopped) || 0;
    var counts = [];
    if (downloading > 0)
        counts.push(downloading + " downloading");
    if (seeding > 0)
        counts.push(seeding + " seeding");
    if (stopped > 0)
        counts.push(stopped + " stopped");
    var vpnText, vpnTone;
    if (!vpn.required) {
        vpnText = "No VPN required";
        vpnTone = "off";
    } else if (s.protected) {
        vpnText = "Only through " + vpn.interface;
        vpnTone = "ok";
    } else if (!vpn.up) {
        vpnText = "VPN down: torrents paused";
        vpnTone = "warn";
    } else {
        vpnText = "Binding to " + vpn.interface + "…";
        vpnTone = "warn";
    }
    return {
        running: running,
        engine: s.engine || "unknown",
        busy: running && (downloading > 0 || Number(s.dl) > 0),
        warn: running && vpn.required === true && s.protected !== true,
        speeds: "↓ " + speed(s.dl) + "   ↑ " + speed(s.up),
        counts: !running ? (s.engine === "missing" ? "The torrent engine isn't installed" : "The torrent engine is stopped")
                : counts.length > 0 ? counts.join(" · ") : "No torrents",
        vpn: vpnText,
        vpnTone: vpnTone,
        canPause: running && downloading + seeding > 0,
        canResume: running && stopped > 0,
    };
}

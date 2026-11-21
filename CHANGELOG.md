# Changelog

## 0.1.0 — 2026-11-21

First release.

- A Qt Quick window that follows uQ's theme live (shared uq-theme module 1.0.0), driving
  qbittorrent-nox (or qbittorrent-enhanced-nox) in its own profile over the Web API on
  127.0.0.1, with a random password only this user can read.
- Opens magnet links and .torrent files (command line, drop, paste, file dialog); add sheet with
  name, size, folder and start-now; one window per session over D-Bus.
- List with filters, search, totals and free space; details with files to skip, trackers and
  every action (start, stop, open folder, copy magnet, recheck, reannounce, remove with or
  without files).
- Keeps seeding after the window closes (setting); `uqbit-engine.service` to start at login.
- "Download finished" notifications from the engine, even with the window closed.
- `uqbit config|default|engine|add|list` and a uQ Settings page (Apps › Torrents).

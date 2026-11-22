# Changelog

## 0.5.1 — 2026-11-22

- **uQbit's problems show in uQ.** uQbit uses uQ's shared uq-diag module (`src/uqdiag.*`, uq-diag 1.0.0): a crash of the window or the CLI leaves a crash record (with a backtrace) that uQ 0.3.35 lists on Settings › Problems with a report to copy, and the engine's start-up errors log in uQ's format with codes (`UQBIT-001` the engine isn't installed, `UQBIT-002` its profile couldn't be set up, `UQBIT-003` it couldn't start) that `uqctl explain`, `uqctl log` (which reads `uqbit-engine.service`) and the Problems page explain from `/usr/share/uq/diag/uqbit.toml`. CI checks the vendored copy and that the codes and the list agree.

## 0.5.0 — 2026-11-22

- **Tracker lists.** Add links to public tracker lists (such as ngosang/trackerslist), up to
  10 of them (`tracker_lists`, space-separated, or the Tracker lists card under Transfers in
  uQ Settings). uQbit downloads them, merges them without repeats (at most 500 trackers) and
  has the engine add them to new public torrents, so those find more peers; private torrents
  keep only their own trackers. They refresh every day (a failed refresh keeps the last list
  and tries again in an hour). The engine's own "trackers from a URL" takes only one link and
  needs qBittorrent 5.1, so uQbit does the fetching itself. `uqbit trackers status|update`,
  and `uqbit status --json` has a `trackers` object.

## 0.4.0 — 2026-11-22

Tabs in uQ Settings, startup and the tray.

- **The Torrents page in uQ Settings has tabs**: General, Startup, Folders, Transfers and
  Privacy, instead of one long list. Settings remembers the last one, and searching a setting
  opens its tab.
- **Open uQbit when you log in** (`launch_on_login`): an XDG autostart entry
  (`~/.config/autostart/io.github.asafelobotomy.uqbit.desktop`), which uQ's Startup page lists
  too. **Start in the tray** (`start_in_tray`) opens it as `uqbit --hidden`: no window, just the
  tray icon; a second start at login leaves the running one alone.
- **Tray icon** (a StatusNotifierItem, shown in uQ's Tray island) while any tray setting is
  on: click it to show or hide the window; its menu shows the window, pauses or resumes every
  torrent, or quits. Its tooltip has the speeds. **Close to the tray** (`close_to_tray`) and
  **minimise to the tray** (`minimize_to_tray`) hide the window there instead. With no tray to
  hold it, closing quits and a hidden start opens the window, so uQbit is never out of reach.
- uQbit's window is now a QApplication (Qt Widgets, part of qt6-base) for the tray menu.
- Moves like the rest of uQ: uQ's motion speeds and easing for the switch, toast, buttons and
  progress line, and colours fade on hover.

## 0.3.0 — 2026-11-22

Folder and torrent management in uQ Settings (Apps › Torrents) and `uqbit config`.

- **Folders card.** The save folder, a folder for unfinished downloads (`incomplete_folder`;
  finished torrents move to the save folder), a watched folder whose .torrent files are added
  by themselves and then deleted there (`watch_folder`), how each torrent's files land
  (`content_layout`: original, subfolder, no_subfolder), `.!qB` on unfinished files
  (`incomplete_extension`) and reserving disk space up front (`preallocate`).
- **Queue and seeding card.** How many torrents download, seed and run at once
  (`max_active_downloads`, `max_active_uploads`, `max_active_torrents`; qBittorrent's 3, 3 and
  5 by default, 0 for no limit, all 0 turns the queue off), a seeding time limit (`seed_time`,
  minutes) beside the ratio, and whether a torrent that is done seeding stops or leaves the
  list with its files kept (`when_seeded`).
- The sandboxed engine may write to the unfinished and watched folders too. Changing any folder
  with `uqbit config set` (or the page) updates the sandbox and restarts the engine once when
  it needs a new folder; before, a new save folder set there wasn't writable until uQbit's
  window restarted the engine.

## 0.2.0 — 2026-11-22

Privacy, search, a sandbox and uQ's bar.

- **VPN kill switch.** "Only through the VPN" binds the engine to the VPN's connection
  (NetworkManager's WireGuard device, else an up tunnel such as wg*, tun*, proton*; Tailscale
  doesn't count). While the VPN is down the engine is bound to nothing, so torrents stop instead
  of leaking; changing the binding drops the connections the engine already had. The window,
  `uqbit status` and the uQ widgets say whether torrents are protected right now.
- **Private by default:** peers must encrypt, anonymous mode is on, and peers on a public
  blocklist of monitoring and anti-piracy addresses (Naunter's BT_BlockLists, refreshed weekly)
  are blocked. LAN and loopback ranges are taken out of the list so local peers still work.
- **Search** through qBittorrent's search plugins (needs Python): results as they come, best
  seeded first, added through the plugin when the result is a page. A Plugins sheet adds
  qBittorrent's own plugins in one click, any other by its https link, turns them on and off and
  updates them.
- **Sandboxed engine.** `uqbit-engine.service` sees the system and the home folder read-only and
  may write only its profile, uQbit's state folder, the download folder and folders torrents were
  added to (uQbit keeps a `folders.conf` drop-in for them and restarts the engine when a new one
  is needed). No new privileges, no capabilities, a system-call filter. Restarts on a crash (at
  most 5 times in 5 minutes), runs at low CPU and I/O priority, throttled past 2 GiB.
- **uQ integration:** an icon in uQ's Status island (filled while downloading, a warning dot
  while the VPN holds torrents back) whose popup has the speeds, the counts, the VPN state,
  Pause all, Resume all and Open uQbit; a Torrents tile in quick settings with a switch; and a
  Privacy card on the Settings page.
- `uqbit status [--json]`, `pause-all`, `resume-all`, `show` and `blocklist status|update`.
- Tested with qBittorrent Enhanced Edition's engine (qbittorrent-enhanced-nox 5.2.4.10), which
  Bindle now installs for uQbit.

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

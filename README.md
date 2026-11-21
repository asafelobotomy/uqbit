# uQbit

uQ's torrent app: qBittorrent's engine running in the background, driven from a Qt Quick window
that follows the uQ theme.

uQbit doesn't reimplement BitTorrent. It runs `qbittorrent-nox` (or qBittorrent Enhanced
Edition's `qbittorrent-enhanced-nox`) in a profile of its own and talks to it over its Web API
on 127.0.0.1, so you get qBittorrent's engine with a window that looks like the rest of uQ.

![The torrent list](docs/screenshots/torrents.png)

## What it does

- **Opens magnet links and .torrent files** from the browser, Files, the command line or a drop
  on the window, and can be the default app for both (`uqbit default set`, or the button in
  the window and in uQ Settings).
- **Add sheet**: the torrent's name, size and file count before it's added, where it saves
  (`~/Downloads` by default) and whether it starts now. Turn it off to add straight away.
- **List**: progress, speed and time left for each torrent, filtered by All, Downloading,
  Seeding, Finished, Stopped and Errors, with search. Totals and free space at the bottom left.
- **Details**: start or stop, open the folder, copy the magnet link, check the files again,
  remove (with or without the files), pick which files to download, trackers.
- **Keeps seeding after the window closes** (or not: a setting), and can start the engine at
  login through a systemd user service (`uqbit-engine.service`).
- **Notifies you when a download finishes**, even with the window closed (the engine runs
  `uqbit notify-finished`).
- **Speed limits and a seeding ratio** that stops a torrent once it has shared enough.
- **One window**: opening a link while uQbit is open adds it in that window.
- **uQ Settings page** (Apps › Torrents): default app, engine status with start and stop,
  start on login, download folder, adding, limits.

![Details of a torrent](docs/screenshots/details.png)
![Adding a torrent, with a light uQ theme](docs/screenshots/add-light.png)

## Keys

| Key | Does |
|---|---|
| Ctrl+O | open .torrent files |
| Ctrl+L | add magnet links |
| Ctrl+V | add the magnet links on the clipboard |
| Ctrl+F | search |
| Space | start / stop the selected torrent |
| Delete | remove the selected torrent |
| Ctrl+1…4 | All, Downloading, Seeding, Finished |
| Esc | close the details |
| Ctrl+Q | quit (the engine keeps running unless you turned that off) |

## Command line

```
uqbit [MAGNET|FILE.torrent|URL]...    open the window (and add these)
uqbit add [--folder DIR] [--stopped] MAGNET|FILE|URL...
uqbit list [--json]                   the torrents, newest first
uqbit engine status|start|stop [--json]
uqbit engine run                      the engine in the foreground (what the systemd unit runs)
uqbit config show [--json]            every setting with its value and default
uqbit config set KEY VALUE            download_folder, ask_before_adding, start_added,
                                      notify_finished, keep_running, start_on_login (true/false),
                                      download_limit, upload_limit (KiB/s, 0 = none),
                                      seed_ratio (0 = never stop), webui_port
uqbit config reset KEY
uqbit default status [--json]         does uQbit open .torrent files and magnet links?
uqbit default set
```

## How it runs the engine

- Profile: `~/.local/share/uqbit` (qBittorrent keeps its own settings, torrents and resume data
  there), so it never shares state with a qBittorrent you run yourself.
- WebUI: 127.0.0.1 only, port 17845 by default (`webui_port`), user `uqbit`, with a random
  password in `~/.local/state/uqbit/webui-password` (mode 0600). uQbit writes its PBKDF2 hash
  into the profile before each start; nothing else in the profile is touched.
- Start: `systemctl --user start uqbit-engine.service` when systemd's user manager is there,
  else `uqbit engine run` detached (log in `~/.local/state/uqbit/engine.log`).
- uQbit's settings (folder, limits, ratio, the "finished" notification) are pushed to the engine
  when it connects and whenever they change.

## Build

Qt 6.5+ (Core, Gui, Quick, QuickControls2, DBus, Network) and CMake:

```
cmake -B build -G Ninja && cmake --build build && ctest --test-dir build
```

Runtime: `qbittorrent-nox` 4.6 or newer (5.x preferred) or `qbittorrent-enhanced-nox`.
On Arch: `paru -S uqbit` (AUR).

## Licence

GPL-3.0-or-later.

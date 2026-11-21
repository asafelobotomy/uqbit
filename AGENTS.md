# Agent notes for uQbit

| Path | What |
|---|---|
| `src/config, engine, client, torrents, torrentfile, defaultapp, notify, cli` | `uqbit_core`: no window. The CLI and `tests/core` use only this |
| `src/engine` | the engine's profile, WebUI password (PBKDF2 hash like qBittorrent's), start/stop through systemd or detached |
| `src/client` | qBittorrent Web API v2 client (logs in by itself; `wait()` for the CLI) |
| `src/torrents` | `TorrentModel`: merges sync/maindata, filters and searches |
| `src/app`, `src/single` | the `App` QML singleton; one window per session over D-Bus |
| `qml/` | the window; pure helpers in `qml/logic.js` (Node-tested) |
| `uq/pages/uqbit` | the uQ Settings page (uq_api 2); every command is a `uqbit` subcommand |

Rules: the engine's WebUI listens on 127.0.0.1 only and needs the password; never turn on
`LocalHostAuth=false` or bind elsewhere. Never touch a profile other than uQbit's own. qBittorrent
5 renamed pause/resume to stop/start: keep the fallback. Every text the uQ page shows goes
through `UqLabel`. Keep files under ~400 lines.

Verify: `cmake --build build && ctest --test-dir build && node --test 'tests/**/*.test.mjs' &&
qmltestrunner -import tests/uq/stub -input tests/uq/tst_page.qml && sh tests/gui/smoke.sh build`
(the smoke test needs qbittorrent-nox and runs as a normal user).

Release: bump `VERSION` (and the metainfo release) in a PR; merging it tags vX.Y.Z and runs
release.yml, whose `aur` artifact holds the AUR files. The AUR push is manual.

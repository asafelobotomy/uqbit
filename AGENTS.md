# Agent notes for uQbit

| Path | What |
|---|---|
| `src/config, engine, client, torrents, torrentfile, defaultapp, notify, privacy, cli, cli_status` | `uqbit_core`: no window. The CLI and `tests/core` use only this |
| `src/engine` | the engine's profile, WebUI password (PBKDF2 hash like qBittorrent's), start/stop through systemd or detached |
| `src/client` | qBittorrent Web API v2 client (logs in by itself; `wait()` for the CLI) |
| `src/torrents` | `TorrentModel`: merges sync/maindata, filters and searches |
| `src/privacy` | VPN interface detection and state, blocklist download/format/private-range filter |
| `src/engine` (sandbox) | the systemd drop-in listing the folders the engine may write |
| `src/app`, `src/single`, `src/protect`, `src/search` | the `App` QML singleton (protect: VPN binding loop; search: search API and plugins); one window per session over D-Bus |
| `qml/` | the window; pure helpers in `qml/logic.js` (Node-tested) |
| `uq/pages/uqbit` | the uQ Settings page (uq_api 2); every command is a `uqbit` subcommand |
| `uq/widgets/uqbit`, `uqbit-quick` | uQ bar icon and quick tile (uq_api 1), from `uqbit status --json`; `logic.js`/`Caption.qml` are copies, keep them equal |
| `tests/sandbox/run.sh` | runs the real unit under `systemd --user` (CI job `sandbox`, Ubuntu) |

Rules: the engine's WebUI listens on 127.0.0.1 only and needs the password; never turn on
`LocalHostAuth=false` or bind elsewhere. Never touch a profile other than uQbit's own. qBittorrent
5 renamed pause/resume to stop/start: keep the fallback. Every text the uQ page shows goes
through `UqLabel`, every widget text through `ctx.UqText`. With `vpn_only` on, the engine must
stay bound to the VPN or to `uqbit-novpn`, never to all interfaces. Keep the blocklist's private
range filter (lists block 127/8 and the LAN). Keep files under ~400 lines.

Verify: `cmake --build build && ctest --test-dir build && node --test 'tests/**/*.test.mjs' &&
qmltestrunner -import tests/uq/stub -input tests/uq/tst_page.qml && qmltestrunner -input
tests/uq/tst_widget.qml && sh tests/gui/smoke.sh build` and `uqctl integrations check uq/
--no-programs`
(the smoke test needs qbittorrent-nox and runs as a normal user).

Release: bump `VERSION` (and the metainfo release) in a PR; merging it tags vX.Y.Z and runs
release.yml, whose `aur` artifact holds the AUR files. The AUR push is manual.

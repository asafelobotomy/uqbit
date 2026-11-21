#!/bin/sh
# Runs the engine's systemd user unit (packaging/systemd/uqbit-engine.service) as it ships, with
# its sandbox, under a real user manager, and checks the sandbox does what the unit says:
#   - the engine starts and answers on 127.0.0.1;
#   - it writes its profile and the download folder (a path with a space, as uQbit's
#     folders.conf drop-in quotes it), and nothing else in the home folder;
#   - Python's multiprocessing (search plugins) works inside it.
# uqbit itself isn't needed: a drop-in runs qbittorrent-nox directly with a profile written here.
# Needs: systemd --user (lingering), qbittorrent-nox, python3, curl. CI: the "sandbox" job.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
unit="$here/../../packaging/systemd/uqbit-engine.service"
cfg="$HOME/.config/systemd/user"
profile="$HOME/.local/share/uqbit"
state="$HOME/.local/state/uqbit"
dl="$HOME/My Downloads"
port=17846

fail() { echo "FAIL: $*"; systemctl --user status uqbit-engine --no-pager || true;
         journalctl --user -u uqbit-engine --no-pager | tail -40 || true; exit 1; }

mkdir -p "$cfg/uqbit-engine.service.d" "$profile/qBittorrent/config" "$state" "$dl"
rm -f "$HOME/.forbidden" "$dl"/.written "$dl"/.python
cp "$unit" "$cfg/uqbit-engine.service"
# What uQbit writes (src/engine.cpp, sandboxDropIn).
cat > "$cfg/uqbit-engine.service.d/folders.conf" <<CONF
[Service]
ReadWritePaths="-$profile"
ReadWritePaths="-$state"
ReadWritePaths="-$dl"
ReadWritePaths="-$HOME/Not There"
CONF
# For the test only: the engine without uqbit, then probes run inside the same sandbox.
cat > "$cfg/uqbit-engine.service.d/test.conf" <<CONF
[Service]
ExecStart=
ExecStart=/usr/bin/qbittorrent-nox --profile=$profile --webui-port=$port
ExecStartPost=/bin/sh -c 'touch "$dl/.written"; touch "$HOME/.forbidden" 2>/dev/null; touch /etc/uqbit-forbidden 2>/dev/null; exit 0'
ExecStartPost=/bin/sh -c 'python3 -c "import multiprocessing as m; q = m.SimpleQueue(); q.put(1); print(q.get())" > "$dl/.python" 2>&1; exit 0'
CONF
cat > "$profile/qBittorrent/config/qBittorrent.conf" <<CONF
[LegalNotice]
Accepted=true

[Preferences]
WebUI\Enabled=true
WebUI\Address=127.0.0.1
WebUI\Port=$port
WebUI\LocalHostAuth=false
CONF

systemctl --user daemon-reload
systemctl --user start uqbit-engine.service || fail "the sandboxed unit starts"
i=0
until curl -sf "http://127.0.0.1:$port/api/v2/app/version" >/dev/null; do
    i=$((i + 1)); [ $i -lt 60 ] || fail "the engine answers"; sleep 0.5
done
echo "engine $(curl -s "http://127.0.0.1:$port/api/v2/app/version") answers inside the sandbox"

[ -f "$dl/.written" ] || fail "the download folder (with a space in its name) is writable"
[ ! -e "$HOME/.forbidden" ] || fail "the rest of the home folder is read-only"
[ ! -e /etc/uqbit-forbidden ] || fail "the system is read-only"
grep -qx 1 "$dl/.python" || { cat "$dl/.python"; fail "Python multiprocessing works (search plugins)"; }
# The engine wrote its own profile (logs, settings) and nothing outside it.
[ -d "$profile/qBittorrent/data" ] || ls -R "$profile" | head
props=$(systemctl --user show uqbit-engine -p ProtectSystem -p ProtectHome -p NoNewPrivileges -p MemoryHigh -p Restart)
echo "$props"
echo "$props" | grep -q '^ProtectSystem=strict' || fail "ProtectSystem=strict holds"
echo "$props" | grep -q '^Restart=on-failure' || fail "it restarts on failure"

# Auto-restart: a crash brings it back.
pid=$(systemctl --user show -p MainPID --value uqbit-engine)
kill -SEGV "$pid"
sleep 8
[ "$(systemctl --user show -p MainPID --value uqbit-engine)" != "$pid" ] || fail "a crash restarts it"
systemctl --user is-active --quiet uqbit-engine || fail "it is running again after the crash"

systemctl --user stop uqbit-engine
echo "sandbox: all checks passed"

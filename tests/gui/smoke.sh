#!/bin/sh
# Starts uQbit under Xvfb on a private session bus with a real qbittorrent-nox, and checks the
# whole path: the window starts the engine, a .torrent opened on the command line is added (its
# data is already there, so it checks to 100% and seeds), a magnet link opened by a second
# `uqbit` goes to the same window over D-Bus, the engine keeps running after the window closes,
# and `uqbit engine stop` stops it. Exits non-zero on any QML error or failed check.
# Needs: qbittorrent-nox, xorg-server-xvfb, dbus, python, imagemagick (for SHOTS_DIR).
#
#   sh tests/gui/smoke.sh BUILD_DIR [SHOTS_DIR]
set -eu
build=$(cd "${1:?usage: smoke.sh BUILD_DIR [SHOTS_DIR]}" && pwd)
shots=${2:-}
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap '"$build/uqbit" engine stop >/dev/null 2>&1 || true; kill $(jobs -p) 2>/dev/null || true; rm -rf "$work"' EXIT

export HOME="$work/home" XDG_CONFIG_HOME="$work/home/.config" XDG_STATE_HOME="$work/home/.local/state"
export XDG_DATA_HOME="$work/home/.local/share" UQBIT_NO_SYSTEMD=1
export LANG=C.UTF-8 DISPLAY=:96 QT_QPA_PLATFORM=xcb QT_FORCE_STDERR_LOGGING=1
# The engine is on 127.0.0.1; never through a proxy.
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY all_proxy ALL_PROXY
mkdir -p "$HOME/Downloads" "$work/t"
"$build/uqbit" config set webui_port 17999 >/dev/null
"$build/uqbit" config set ask_before_adding false >/dev/null
# No blocklist download from the internet in a test.
"$build/uqbit" config set blocklist false >/dev/null

head -c 3000000 /dev/urandom > "$HOME/Downloads/smoke-data.bin"
python3 "$here/mktorrent.py" "$work/t/smoke.torrent" "$HOME/Downloads/smoke-data.bin" > "$work/t/hash"
hash=$(cat "$work/t/hash")

fail() { echo "FAIL: $*"; echo "--- uqbit's output"; cat "$work/log" 2>/dev/null; echo "--- engine log";
         cat "$XDG_STATE_HOME/uqbit/engine.log" 2>/dev/null; exit 1; }
wait_for() { # seconds, description, command...
    t=$1; what=$2; shift 2
    i=0
    until "$@" >/dev/null 2>&1; do
        i=$((i + 1)); [ $i -lt $((t * 4)) ] || fail "$what"; sleep 0.25
    done
}
json() { "$build/uqbit" "$@" --json; }

json engine status | grep -q '"installed":true' || fail "qbittorrent-nox is installed"
json engine status | grep -q '"running":false' || fail "the engine is stopped before the window opens"

Xvfb :96 -screen 0 1180x720x24 >/dev/null 2>&1 &
sleep 1
eval "$(dbus-launch --sh-syntax)"
"$build/uqbit" "$work/t/smoke.torrent" >"$work/log" 2>&1 &
pid=$!

wait_for 30 "the window starts the engine" sh -c "\"$build/uqbit\" engine status --json | grep -q '\"running\":true'"
wait_for 30 "the .torrent is added" sh -c "\"$build/uqbit\" list --json | grep -q $hash"
wait_for 30 "it checks to 100% and seeds" sh -c "\"$build/uqbit\" list --json | grep -q '\"progress\":1'"

# A second uqbit hands its link to this window and exits.
"$build/uqbit" "magnet:?xt=urn:btih:08ada5a7a6183aae1e09d831df6748d566095a10&dn=Smoke+Magnet" >"$work/log2" 2>&1 &
second=$!
wait_for 10 "the second uqbit exits" sh -c "! kill -0 $second"
kill -0 $pid || fail "the first window is still open"
wait_for 20 "the magnet is added through the open window" sh -c "\"$build/uqbit\" list --json | grep -q 08ada5a7a6183aae1e09d831df6748d566095a10"
[ "$(json list | grep -o '"hash"' | wc -l)" -eq 2 ] || fail "two torrents"

sleep 2
[ -n "$shots" ] && { mkdir -p "$shots"; import -window root "$shots/smoke.png"; }

# Privacy: encryption and anonymous mode reach the engine; the VPN rule binds it to an
# interface that doesn't exist while no VPN is up (the kill switch) and back to any after.
json status | grep -q '"engine":"running"' || fail "uqbit status sees the engine"
json status | grep -q '"encryption":"require"' || fail "encryption is required by default"
"$build/uqbit" config set vpn_only true >/dev/null
json status | grep -q '"required":true' || fail "the VPN rule is on"
json status | grep -q '"bound":true' || fail "the engine is bound for the VPN rule"
json status | grep -q '"protected":false' || fail "with no VPN up, nothing is protected (and nothing is sent)"
"$build/uqbit" config set vpn_only false >/dev/null
json status | grep -q '"required":false' || fail "the VPN rule is off again"
"$build/uqbit" pause-all | grep -q stopped || fail "pause-all"
"$build/uqbit" resume-all | grep -q started || fail "resume-all"

# Closing the window leaves the engine seeding (keep_running defaults to on).
kill $pid
wait_for 10 "the window closes" sh -c "! kill -0 $pid"
json engine status | grep -q '"running":true' || fail "the engine keeps running after the window closes"
"$build/uqbit" engine stop >/dev/null
json engine status | grep -q '"running":false' || fail "uqbit engine stop stops it"

if grep -E "ReferenceError|TypeError|Unable to assign|is not a type|is not installed|Cannot read" "$work/log"; then
    fail "QML errors in the log"
fi
echo "smoke test passed"

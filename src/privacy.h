// What keeps torrenting private: the VPN kill switch (the engine bound to the VPN's interface,
// so nothing leaves while the VPN is down) and the peer blocklist.
#pragma once

#include "config.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace uqbit {

/// The interface the engine binds to while the VPN it should use is unknown: no such interface
/// exists, so libtorrent can neither listen nor connect (nothing leaks), and qBittorrent picks
/// the real one up as soon as uQbit rebinds it.
inline constexpr auto kNoVpn = "uqbit-novpn";

struct NetIf {
    QString name;
    bool up = false;   // IFF_UP and running
    int arpType = 0;   // /sys/class/net/<name>/type (65534: no link layer, as tun and WireGuard)
};

/// The VPN interface among `ifs`: NetworkManager's active VPN or WireGuard device first
/// (`nmDevices`, in NetworkManager's order), else the first up interface that looks like one
/// (wg*, tun*, tap*, ppp*, proton*, nordlynx, mullvad*, ipsec*, or no link layer). Tailscale is
/// a mesh, not a way out, so it never counts. Empty when there is none.
QString pickVpn(const QList<NetIf> &ifs, const QStringList &nmDevices = {});

/// This machine's interfaces and NetworkManager's active VPN devices, then pickVpn().
QString detectVpn();

struct VpnState {
    bool required = false;  // vpn_only
    QString interface;      // what the engine should be bound to ("" when not required)
    bool up = false;        // that interface exists and is up
};

/// What the engine should be bound to now. With vpn_only: the configured interface, else the
/// detected VPN, else the last one seen (kept in the state dir, so the engine waits for it to
/// come back), else kNoVpn. `detected` overrides detection (tests).
VpnState vpnState(const Config &config, const QString *detected = nullptr);

/// The engine's bound interface (qBittorrent's current_network_interface) against what it
/// should be: `{required, interface, up, bound}` plus `protected` (required, up and bound).
/// The shape `uqbit status --json` and uQ's VPN island read; keep it stable.
QJsonObject vpnJson(const VpnState &state, const QString &engineInterface, bool engineKnown);

// ---- blocklist

/// <state>/blocklist.<p2p|dat|p2b>, whichever was stored last; "" when none was.
QString blocklistPath();
/// Gunzips `raw` when it is gzipped, works out its format (P2B by its magic, DAT by "a - b ,"
/// lines, else P2P "name:a-b"), checks that it holds ranges, and stores it in the state dir
/// (replacing the last one) without private ranges. Returns the path, or "" with `error` set.
QString storeBlocklist(const QByteArray &raw, QString *error);
/// The list (p2p or dat) without the ranges that touch this machine, the LAN or link-local
/// addresses: some public lists block them, which would cut off LAN peers.
QByteArray dropPrivateRanges(const QByteArray &data, const QString &format, int *dropped);
/// Whether the stored list is missing or older than a week.
bool blocklistStale();
/// Gunzip (RFC 1952), at most `limit` bytes out. Empty with `error` set on failure.
QByteArray gunzip(const QByteArray &in, qsizetype limit, QString *error);

} // namespace uqbit

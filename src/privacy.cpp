#include "privacy.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <zlib.h>

#include <array>

namespace uqbit {

static bool looksLikeVpn(const NetIf &i)
{
    static const QRegularExpression name(
        "^(wg|tun|tap|ppp|proton|nordlynx|nordtun|mullvad|ipsec|vpn|pvpn|expressvpn|"
        "surfshark|windscribe|cyberghost|ovpn)");
    if (i.name.startsWith("tailscale") || i.name == "lo")
        return false;
    return name.match(i.name).hasMatch() || i.arpType == 65534;
}

QString pickVpn(const QList<NetIf> &ifs, const QStringList &nmDevices)
{
    for (const QString &dev : nmDevices)
        for (const NetIf &i : ifs)
            if (i.name == dev && i.up && !i.name.startsWith("tailscale"))
                return i.name;
    for (const NetIf &i : ifs)
        if (i.up && looksLikeVpn(i))
            return i.name;
    return {};
}

static QStringList nmVpnDevices()
{
    if (QStandardPaths::findExecutable("nmcli").isEmpty())
        return {};
    QProcess p;
    p.start("nmcli", {"-t", "-f", "TYPE,DEVICE", "connection", "show", "--active"});
    if (!p.waitForFinished(1500)) {
        p.kill();
        p.waitForFinished(200);
        return {};
    }
    QStringList out;
    for (const QByteArray &line : p.readAllStandardOutput().split('\n')) {
        const int colon = line.lastIndexOf(':');
        if (colon < 0)
            continue;
        const QByteArray type = line.left(colon), dev = line.mid(colon + 1);
        // A "vpn" connection (OpenVPN, IPsec...) names the device it rides on, not its tunnel:
        // only WireGuard's device is the tunnel itself. The tunnels show up by their names.
        if (type == "wireguard" && !dev.isEmpty())
            out << QString::fromUtf8(dev);
    }
    return out;
}

QString detectVpn()
{
    QList<NetIf> ifs;
    for (const QNetworkInterface &n : QNetworkInterface::allInterfaces()) {
        NetIf i;
        i.name = n.name();
        i.up = n.flags().testFlag(QNetworkInterface::IsUp)
               && n.flags().testFlag(QNetworkInterface::IsRunning);
        QFile t("/sys/class/net/" + i.name + "/type");
        if (t.open(QIODevice::ReadOnly))
            i.arpType = t.readAll().trimmed().toInt();
        ifs << i;
    }
    return pickVpn(ifs, nmVpnDevices());
}

static QString lastVpnPath() { return stateDir() + "/vpn-interface"; }

static bool interfaceUp(const QString &name)
{
    const QNetworkInterface n = QNetworkInterface::interfaceFromName(name);
    return n.isValid() && n.flags().testFlag(QNetworkInterface::IsUp);
}

VpnState vpnState(const Config &config, const QString *detected)
{
    VpnState s;
    s.required = config.boolean("vpn_only");
    if (!s.required)
        return s;
    const QString fixed = config.text("vpn_interface");
    if (!fixed.isEmpty()) {
        s.interface = fixed;
        s.up = detected ? *detected == fixed : interfaceUp(fixed);
        return s;
    }
    const QString found = detected ? *detected : detectVpn();
    QFile last(lastVpnPath());
    if (!found.isEmpty()) {
        s.interface = found;
        s.up = true;
        if (!detected) {
            QDir().mkpath(stateDir());
            QSaveFile out(lastVpnPath());
            if (out.open(QIODevice::WriteOnly)) {
                out.write(found.toUtf8() + '\n');
                out.commit();
            }
        }
        return s;
    }
    if (!detected && last.open(QIODevice::ReadOnly)) {
        const QString name = QString::fromUtf8(last.read(64)).trimmed();
        if (QRegularExpression("^[A-Za-z0-9_.:@-]{1,15}$").match(name).hasMatch()) {
            s.interface = name;
            return s;
        }
    }
    s.interface = QString::fromLatin1(kNoVpn);
    return s;
}

QJsonObject vpnJson(const VpnState &state, const QString &engineInterface, bool engineKnown)
{
    const bool bound = engineKnown && state.required && engineInterface == state.interface;
    QJsonObject vpn{{"required", state.required},
                    {"interface", state.required && state.interface != QLatin1String(kNoVpn)
                                      ? QJsonValue(state.interface)
                                      : QJsonValue(QJsonValue::Null)},
                    {"up", state.up},
                    {"bound", bound}};
    return {{"vpn", vpn}, {"protected", bound && state.up}};
}

// ---- blocklist

static const QStringList kFormats = {"p2p", "dat", "p2b"};

QString blocklistPath()
{
    QString best;
    QDateTime newest;
    for (const QString &ext : kFormats) {
        const QFileInfo f(stateDir() + "/blocklist." + ext);
        if (f.isFile() && (best.isEmpty() || f.lastModified() > newest)) {
            best = f.absoluteFilePath();
            newest = f.lastModified();
        }
    }
    return best;
}

bool blocklistStale()
{
    const QString p = blocklistPath();
    return p.isEmpty() || QFileInfo(p).lastModified().daysTo(QDateTime::currentDateTime()) >= 7;
}

QByteArray gunzip(const QByteArray &in, qsizetype limit, QString *error)
{
    z_stream z{};
    if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK) {
        *error = QStringLiteral("can't start gunzip");
        return {};
    }
    z.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    z.avail_in = uInt(in.size());
    QByteArray out;
    char buf[64 * 1024];
    int rc = Z_OK;
    while (rc != Z_STREAM_END) {
        z.next_out = reinterpret_cast<Bytef *>(buf);
        z.avail_out = sizeof buf;
        rc = inflate(&z, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) {
            inflateEnd(&z);
            *error = QStringLiteral("the blocklist is not valid gzip");
            return {};
        }
        out.append(buf, qsizetype(sizeof buf - z.avail_out));
        if (out.size() > limit) {
            inflateEnd(&z);
            *error = QStringLiteral("the blocklist is too big");
            return {};
        }
        if (rc == Z_OK && z.avail_in == 0 && z.avail_out != 0) {
            inflateEnd(&z);
            *error = QStringLiteral("the blocklist is cut short");
            return {};
        }
    }
    inflateEnd(&z);
    return out;
}

// ---- private ranges

namespace {
using Addr = std::array<quint8, 16>; // IPv4 in the first 4 bytes, the rest 0

bool parseAddr(QByteArrayView textIn, bool *v6, Addr *out)
{
    // DAT lists pad IPv4 with zeros ("001.002.003.000"), which QHostAddress refuses.
    const QByteArrayView text = textIn.trimmed();
    const QList<QByteArray> parts = text.toByteArray().split('.');
    if (parts.size() == 4) {
        out->fill(0);
        for (int i = 0; i < 4; ++i) {
            bool ok = false;
            const uint n = parts.at(i).toUInt(&ok);
            if (!ok || parts.at(i).size() > 3 || n > 255)
                return false;
            (*out)[i] = quint8(n);
        }
        *v6 = false;
        return true;
    }
    QHostAddress a;
    if (!a.setAddress(QString::fromLatin1(text.trimmed())))
        return false;
    out->fill(0);
    if (a.protocol() == QAbstractSocket::IPv4Protocol) {
        *v6 = false;
        const quint32 n = a.toIPv4Address();
        for (int i = 0; i < 4; ++i)
            (*out)[i] = quint8(n >> (24 - 8 * i));
        return true;
    }
    *v6 = true;
    const Q_IPV6ADDR x = a.toIPv6Address();
    std::copy(std::begin(x.c), std::end(x.c), out->begin());
    return true;
}

struct Block {
    bool v6;
    Addr lo, hi;
};

Addr addr(const char *text)
{
    bool v6;
    Addr a;
    parseAddr(text, &v6, &a);
    return a;
}

// Addresses that never belong to a monitoring company: this machine, the LAN, link-local.
// Lists that block them (some do) would cut uQbit off from LAN peers and from itself.
const QList<Block> &privateBlocks()
{
    static const QList<Block> all = {
        {false, addr("0.0.0.0"), addr("0.255.255.255")},
        {false, addr("10.0.0.0"), addr("10.255.255.255")},
        {false, addr("100.64.0.0"), addr("100.127.255.255")},
        {false, addr("127.0.0.0"), addr("127.255.255.255")},
        {false, addr("169.254.0.0"), addr("169.254.255.255")},
        {false, addr("172.16.0.0"), addr("172.31.255.255")},
        {false, addr("192.168.0.0"), addr("192.168.255.255")},
        {true, addr("::"), addr("::1")},
        {true, addr("fc00::"), addr("fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")},
        {true, addr("fe80::"), addr("febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff")},
    };
    return all;
}

bool touchesPrivate(bool v6, const Addr &lo, const Addr &hi)
{
    for (const Block &b : privateBlocks())
        if (b.v6 == v6 && lo <= b.hi && hi >= b.lo)
            return true;
    return false;
}
} // namespace

QByteArray dropPrivateRanges(const QByteArray &data, const QString &format, int *dropped)
{
    *dropped = 0;
    QByteArray out;
    out.reserve(data.size());
    qsizetype start = 0;
    while (start < data.size()) {
        qsizetype end = data.indexOf('\n', start);
        if (end < 0)
            end = data.size();
        const QByteArrayView line(data.constData() + start, end - start);
        start = end + 1;
        QByteArrayView range;
        if (format == "p2p") {
            // "name:a-b". Names may hold ':' and so do IPv6 ranges: take the first ':' after
            // which the rest parses as a range.
            for (qsizetype c = line.indexOf(':'); c >= 0; c = line.indexOf(':', c + 1)) {
                const QByteArrayView rest = line.mid(c + 1);
                const qsizetype d = rest.indexOf('-');
                bool v6;
                Addr x;
                if (d > 0 && parseAddr(rest.left(d), &v6, &x)) {
                    range = rest;
                    break;
                }
            }
        } else {
            const qsizetype comma = line.indexOf(',');
            range = comma < 0 ? line : line.left(comma);
        }
        const qsizetype d = range.indexOf('-');
        bool v6a = false, v6b = false;
        Addr lo, hi;
        if (d > 0 && parseAddr(range.left(d), &v6a, &lo) && parseAddr(range.mid(d + 1), &v6b, &hi)
            && v6a == v6b && touchesPrivate(v6a, lo, hi)) {
            ++*dropped;
            continue;
        }
        out.append(line.data(), line.size());
        out.append('\n');
    }
    return out;
}

QString storeBlocklist(const QByteArray &raw, QString *error)
{
    constexpr qsizetype kLimit = 256 * 1024 * 1024;
    QByteArray data = raw;
    if (data.startsWith("\x1f\x8b")) {
        data = gunzip(raw, kLimit, error);
        if (data.isEmpty())
            return {};
    }
    QString ext;
    if (data.startsWith("\xff\xff\xff\xffP2B")) {
        ext = "p2b";
    } else {
        // Look at the first lines that aren't comments.
        static const QRegularExpression dat(
            "^\\s*\\d{1,3}(\\.\\d{1,3}){3}\\s*-\\s*\\d{1,3}(\\.\\d{1,3}){3}\\s*,");
        static const QRegularExpression p2p(
            ":\\s*[0-9a-fA-F.:]+\\s*-\\s*[0-9a-fA-F.:]+\\s*$");
        int checked = 0;
        for (const QByteArray &l : data.left(64 * 1024).split('\n')) {
            const QString line = QString::fromLatin1(l).trimmed();
            if (line.isEmpty() || line.startsWith('#') || line.startsWith("//"))
                continue;
            if (dat.match(line).hasMatch()) {
                ext = "dat";
                break;
            }
            if (p2p.match(line).hasMatch()) {
                ext = "p2p";
                break;
            }
            if (++checked > 20)
                break;
        }
    }
    if (ext.isEmpty()) {
        *error = QStringLiteral("that is not a blocklist (P2P, DAT or P2B)");
        return {};
    }
    if (ext != "p2b") {
        int dropped = 0;
        data = dropPrivateRanges(data, ext, &dropped);
    }
    QDir().mkpath(stateDir());
    const QString path = stateDir() + "/blocklist." + ext;
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit()) {
        *error = QStringLiteral("can't write %1").arg(path);
        return {};
    }
    for (const QString &other : kFormats)
        if (other != ext)
            QFile::remove(stateDir() + "/blocklist." + other);
    return path;
}

} // namespace uqbit

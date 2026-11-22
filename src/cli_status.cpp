// `uqbit status`, `pause-all`, `resume-all`, `show` and `blocklist`: what uQ's bar widget,
// its VPN island and scripts ask for.
#include "cli_util.h"
#include "engine.h"
#include "privacy.h"
#include "torrents.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>

namespace uqbit {

static QString speed(double bytes)
{
    const char *units[] = {"B/s", "KiB/s", "MiB/s", "GiB/s"};
    int i = 0;
    while (bytes >= 1024 && i < 3) {
        bytes /= 1024;
        ++i;
    }
    return QString::number(bytes, 'f', i == 0 ? 0 : 1) + ' ' + units[i];
}

static QJsonObject blocklistJson(const Config &cfg)
{
    const QString path = blocklistPath();
    return {{"on", cfg.boolean("blocklist")},
            {"path", path.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(path)},
            {"updated", path.isEmpty() ? QJsonValue(QJsonValue::Null)
                                       : QJsonValue(QFileInfo(path).lastModified().toUTC()
                                                        .toString(Qt::ISODate))}};
}

/// Everything `status` says. Rebinds the engine to the VPN when it should be bound elsewhere
/// (the VPN came back under a new name), so a widget polling this keeps the kill switch right.
static QJsonObject status(const Config &cfg)
{
    const VpnState vpn = vpnState(cfg);
    QJsonObject out{{"encryption", cfg.text("encryption")},
                    {"anonymous_mode", cfg.boolean("anonymous_mode")},
                    {"blocklist", blocklistJson(cfg)},
                    {"trackers", trackersJson(cfg)}};
    auto finish = [&](const QString &engine, const QString &bound, bool known) {
        out.insert("engine", engine);
        const QJsonObject v = vpnJson(vpn, bound, known);
        for (auto it = v.begin(); it != v.end(); ++it)
            out.insert(it.key(), it.value());
        return out;
    };
    if (engineBinary().isEmpty())
        return finish("missing", {}, false);
    const QString pw = webPassword();
    Client c(cfg.number("webui_port"), pw);
    const QString version = pw.isEmpty() ? QString() : engineVersion(c, 800);
    if (version.isEmpty()) {
        out.insert("dl", 0);
        out.insert("up", 0);
        return finish("stopped", {}, false);
    }
    out.insert("version", version);

    // Rebinds (and reconnects) when the binding is off; then reads what the engine has.
    c.wait([&](Client::Done d) { bindEngine(&c, vpn.interface, d); }, 6000);
    const Reply prefs = c.wait([&c](Client::Done d) { c.preferences(d); }, 2000);
    const QString bound = QJsonDocument::fromJson(prefs.body).object()
                              .value("current_network_interface").toString();

    const Reply info = c.wait([&c](Client::Done d) { c.get("torrents/info", {}, d); }, 3000);
    double dl = 0, up = 0;
    int downloading = 0, seeding = 0, stopped = 0, errors = 0;
    const QJsonArray list = QJsonDocument::fromJson(info.body).array();
    for (const QJsonValue &v : list) {
        const QJsonObject t = v.toObject();
        dl += t.value("dlspeed").toDouble();
        up += t.value("upspeed").toDouble();
        const QString g = stateGroup(t.value("state").toString());
        downloading += g == "downloading" || g == "checking";
        seeding += g == "seeding";
        stopped += g == "stopped";
        errors += g == "error";
    }
    out.insert("dl", dl);
    out.insert("up", up);
    out.insert("total", list.size());
    out.insert("downloading", downloading);
    out.insert("seeding", seeding);
    out.insert("stopped", stopped);
    out.insert("errors", errors);
    return finish("running", bound, prefs.ok());
}

int cmdStatus(bool asJson, QTextStream &out, QTextStream &)
{
    const QJsonObject s = status(Config());
    if (asJson) {
        json(out, s);
        return 0;
    }
    const QString engine = s.value("engine").toString();
    if (engine == "missing")
        out << "the torrent engine (qbittorrent-nox) is not installed\n";
    else if (engine == "stopped")
        out << "the torrent engine is stopped\n";
    else
        out << QStringLiteral("%1 downloading, %2 seeding · down %3, up %4\n")
                   .arg(s.value("downloading").toInt()).arg(s.value("seeding").toInt())
                   .arg(speed(s.value("dl").toDouble()), speed(s.value("up").toDouble()));
    const QJsonObject vpn = s.value("vpn").toObject();
    if (!vpn.value("required").toBool())
        out << "VPN: not required (torrents use any network)\n";
    else if (s.value("protected").toBool())
        out << "VPN: protected, only through " << vpn.value("interface").toString() << '\n';
    else if (!vpn.value("up").toBool())
        out << "VPN: down, so torrents send nothing until it is back\n";
    else
        out << "VPN: up, but the engine isn't bound to it yet\n";
    return 0;
}

int cmdAll(bool start, QTextStream &out, QTextStream &err)
{
    Config cfg;
    Client c(cfg.number("webui_port"), webPassword());
    if (engineVersion(c, 1500).isEmpty())
        return fail(err, "the torrent engine isn't running");
    const Reply r = c.wait([&](Client::Done d) { c.startStop({"all"}, start, d); }, 5000);
    if (!r.ok())
        return fail(err, r.error);
    out << (start ? "started every torrent\n" : "stopped every torrent\n");
    return 0;
}

int cmdShow(QTextStream &err)
{
    // The window forwards to a running one by itself.
    if (!QProcess::startDetached(self(), {}))
        return fail(err, "can't open the window");
    return 0;
}

int cmdBlocklist(const QStringList &args, bool asJson, QTextStream &out, QTextStream &err)
{
    Config cfg;
    const QString sub = args.value(1);
    if (sub == "status" && args.size() == 2) {
        const QJsonObject b = blocklistJson(cfg);
        if (asJson)
            json(out, b);
        else if (b.value("path").isNull())
            out << "no blocklist yet" << (cfg.boolean("blocklist") ? "" : " (turned off)") << '\n';
        else
            out << b.value("path").toString() << " (updated " << b.value("updated").toString()
                << (cfg.boolean("blocklist") ? ")\n" : "; turned off)\n");
        return 0;
    }
    if (sub == "update" && args.size() == 2) {
        if (!cfg.boolean("blocklist"))
            return fail(err, "the blocklist is turned off (uqbit config set blocklist true)");
        Client c(cfg.number("webui_port"), webPassword());
        const bool running = !engineVersion(c, 1500).isEmpty();
        QString error;
        if (running) {
            // Download, store and have the engine load it again.
            const Reply r = c.wait([&](Client::Done d) {
                updateBlocklist(&c, cfg, [d](const QString &e) { d({200, {}, e}); });
            }, 120000);
            if (!r.ok())
                return fail(err, r.error);
        } else {
            const Reply r = c.wait([&](Client::Done d) {
                c.download(QUrl(cfg.text("blocklist_url")), 64 << 20, d);
            }, 120000);
            if (!r.ok())
                return fail(err, "couldn't download the blocklist: " + r.error);
            if (storeBlocklist(r.body, &error).isEmpty())
                return fail(err, error);
        }
        const QJsonObject b = blocklistJson(cfg);
        if (b.value("path").isNull())
            return fail(err, "couldn't update the blocklist");
        if (asJson)
            json(out, b);
        else
            out << "blocklist updated: " << b.value("path").toString() << '\n';
        return 0;
    }
    return fail(err, "usage: uqbit blocklist status|update", 2);
}

} // namespace uqbit

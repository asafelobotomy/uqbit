#include "cli.h"
#include "cli_util.h"
#include "client.h"
#include "config.h"
#include "defaultapp.h"
#include "engine.h"
#include "notify.h"
#include "privacy.h"
#include "torrentfile.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QUrl>

namespace uqbit {

static const char *kUsage =
    "uqbit " UQBIT_VERSION " - uQ's torrent app\n"
    "\n"
    "Usage:\n"
    "  uqbit [MAGNET|FILE.torrent|URL]...  open the window (and add these)\n"
    "  uqbit add [--folder DIR] [--stopped] MAGNET|FILE|URL...\n"
    "                                      add torrents without the window\n"
    "  uqbit list [--json]                 the torrents, newest first\n"
    "  uqbit status [--json]               speeds, counts and whether the VPN protects them\n"
    "  uqbit pause-all | resume-all        stop or start every torrent\n"
    "  uqbit show                          open the window\n"
    "  uqbit blocklist status|update [--json]\n"
    "                                      the peer blocklist; update downloads it now\n"
    "  uqbit engine status [--json]        is the torrent engine (qbittorrent-nox) running?\n"
    "  uqbit engine start|stop             start or stop it\n"
    "  uqbit engine run                    run it in the foreground (the systemd unit does)\n"
    "  uqbit config show [--json]          every setting with its value and default\n"
    "  uqbit config set KEY VALUE          change one setting\n"
    "  uqbit config reset KEY              back to the default\n"
    "  uqbit default status [--json]       does uQbit open torrent files and magnet links?\n"
    "  uqbit default set                   make it so\n"
    "  uqbit --version | --help\n";

bool isCliCommand(const QStringList &argsIn)
{
    QStringList args = argsIn;
    args.removeAll(QStringLiteral("--json"));
    if (args.isEmpty())
        return false;
    static const QStringList cmds = {"config",    "default",   "engine",    "add",
                                     "list",      "status",    "pause-all", "resume-all",
                                     "show",      "blocklist", "notify-finished",
                                     "--version", "-V",        "--help",    "-h"};
    return cmds.contains(args.first());
}

void json(QTextStream &out, const QJsonObject &o)
{
    out << QJsonDocument(o).toJson(QJsonDocument::Compact) << '\n';
}

int fail(QTextStream &err, const QString &msg, int code)
{
    err << "uqbit: " << msg << '\n';
    return code;
}

QString self() { return QCoreApplication::applicationFilePath(); }

QString engineVersion(Client &c, int timeoutMs)
{
    const Reply r = c.wait([&c](Client::Done d) { c.version(d); }, timeoutMs);
    return r.ok() ? QString::fromUtf8(r.body).trimmed() : QString();
}

QString ensureEngine(Client &c)
{
    if (!engineVersion(c, 2000).isEmpty())
        return {};
    const QString e = startEngine(Config(), self());
    if (!e.isEmpty())
        return e;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 20000) {
        if (!engineVersion(c, 2000).isEmpty()) {
            applyToEngine(Config());
            return {};
        }
        QThread::msleep(300);
    }
    return QStringLiteral("the torrent engine didn't start (see `journalctl --user -u %1`)")
        .arg(QString::fromLatin1(kUnit));
}

static QJsonObject engineStatus(const Config &cfg)
{
    QString pwErr;
    const QString pw = webPassword(&pwErr);
    Client c(cfg.number("webui_port"), pw);
    const QString version = pw.isEmpty() ? QString() : engineVersion(c);
    const QString bin = engineBinary();
    return {{"installed", !bin.isEmpty()},
            {"binary", bin},
            {"running", !version.isEmpty()},
            {"version", version},
            {"webui_port", cfg.number("webui_port")},
            {"profile", profileDir()},
            {"systemd", haveUserUnit()},
            {"start_on_login", cfg.boolean("start_on_login")}};
}

void applyToEngine(const Config &cfg)
{
    Client c(cfg.number("webui_port"), webPassword());
    if (engineVersion(c, 1500).isEmpty())
        return;
    const QJsonObject prefs = preferencesFor(cfg, self());
    c.wait([&](Client::Done d) { c.setPreferences(prefs, d); }, 5000);
    const QString bind = vpnState(cfg).interface;
    c.wait([&](Client::Done d) { bindEngine(&c, bind, d); }, 10000);
}

static int cmdConfig(const QStringList &args, bool asJson, QTextStream &out, QTextStream &err)
{
    Config cfg;
    const QString sub = args.value(1);
    if (sub == "show" && args.size() == 2) {
        if (asJson) {
            json(out, cfg.describe());
        } else {
            for (const Key &k : keys())
                out << k.name << " = " << cfg.value(k.name).toString() << '\n';
        }
        return 0;
    }
    if ((sub == "set" && args.size() == 4) || (sub == "reset" && args.size() == 3)) {
        const QString key = args.at(2);
        const bool wasOn = cfg.boolean("start_on_login");
        const QString e = sub == "set" ? cfg.set(key, args.at(3)) : cfg.reset(key);
        if (!e.isEmpty())
            return fail(err, e, 2);
        if (key == "start_on_login" && cfg.boolean(key) != wasOn) {
            const QString se = setStartOnLogin(cfg.boolean(key));
            if (!se.isEmpty()) {
                cfg.set(key, wasOn ? "true" : "false");
                return fail(err, se);
            }
        }
        if (key != "start_on_login" && key != "webui_port")
            applyToEngine(cfg);
        out << key << " = " << cfg.value(key).toString() << (sub == "reset" ? " (default)\n" : "\n");
        return 0;
    }
    return fail(err, "usage: uqbit config show|set KEY VALUE|reset KEY", 2);
}

static int cmdDefault(const QStringList &args, bool asJson, QTextStream &out, QTextStream &err)
{
    const QString sub = args.value(1);
    if (sub == "status" && args.size() == 2) {
        const QJsonObject s = defaultStatus();
        if (asJson) {
            json(out, s);
        } else {
            out << (s.value("default").toBool() ? "uQbit opens torrent files and magnet links\n"
                                                : "uQbit is not the default for both\n");
            auto name = [](const QString &id) { return id.isEmpty() ? QStringLiteral("none") : id; };
            out << "torrent files: " << name(s.value("torrent").toString())
                << "\nmagnet links: " << name(s.value("magnet").toString()) << '\n';
        }
        return 0;
    }
    if (sub == "set" && args.size() == 2) {
        const QString e = makeDefault();
        if (!e.isEmpty())
            return fail(err, e);
        if (asJson)
            json(out, defaultStatus());
        else
            out << "uQbit now opens torrent files and magnet links\n";
        return 0;
    }
    return fail(err, "usage: uqbit default status|set", 2);
}

static int cmdEngine(const QStringList &args, bool asJson, QTextStream &out, QTextStream &err)
{
    Config cfg;
    const QString sub = args.value(1);
    if (sub == "run" && args.size() == 2)
        return execEngine(cfg);
    if (sub == "status" && args.size() == 2) {
        const QJsonObject s = engineStatus(cfg);
        if (asJson) {
            json(out, s);
        } else if (!s.value("installed").toBool()) {
            out << "qbittorrent-nox is not installed\n";
        } else {
            out << (s.value("running").toBool()
                        ? QStringLiteral("running (qBittorrent %1)\n").arg(s.value("version").toString())
                        : QStringLiteral("stopped\n"));
        }
        return 0;
    }
    if (sub == "start" && args.size() == 2) {
        QString pwErr;
        Client c(cfg.number("webui_port"), webPassword(&pwErr));
        if (!pwErr.isEmpty())
            return fail(err, pwErr);
        const QString e = ensureEngine(c);
        if (!e.isEmpty())
            return fail(err, e);
        applyToEngine(cfg);
        if (asJson)
            json(out, engineStatus(cfg));
        else
            out << "the torrent engine is running\n";
        return 0;
    }
    if (sub == "stop" && args.size() == 2) {
        Client c(cfg.number("webui_port"), webPassword());
        if (!engineVersion(c, 1500).isEmpty())
            c.wait([&c](Client::Done d) { c.shutdown(d); }, 5000);
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 15000 && !engineVersion(c, 1000).isEmpty())
            QThread::msleep(300);
        if (asJson)
            json(out, engineStatus(cfg));
        else
            out << "the torrent engine is stopped\n";
        return 0;
    }
    return fail(err, "usage: uqbit engine status|start|stop|run", 2);
}

static int cmdAdd(QStringList args, QTextStream &out, QTextStream &err)
{
    Config cfg;
    args.removeFirst();
    QString folder = expandHome(cfg.text("download_folder"));
    bool start = cfg.boolean("start_added");
    if (args.removeAll("--stopped") > 0)
        start = false;
    const int f = int(args.indexOf("--folder"));
    if (f >= 0) {
        if (f + 1 >= args.size())
            return fail(err, "--folder needs a folder", 2);
        folder = expandHome(args.at(f + 1));
        args.remove(f, 2);
    }
    args.removeAll("--");
    if (args.isEmpty())
        return fail(err, "usage: uqbit add [--folder DIR] [--stopped] MAGNET|FILE|URL...", 2);
    QStringList urls;
    QList<Upload> files;
    for (const QString &a : args) {
        if (isTorrentLink(a)) {
            urls.append(a.trimmed());
            continue;
        }
        const QString path = a.startsWith("file://") ? QUrl(a).toLocalFile() : a;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024 * 1024)
            return fail(err, QStringLiteral("can't read %1").arg(a));
        const QByteArray data = file.readAll();
        if (!describeTorrent(data).value("ok").toBool())
            return fail(err, QStringLiteral("%1 isn't a torrent file").arg(a));
        files.append({QFileInfo(path).fileName(), data});
    }
    Client c(cfg.number("webui_port"), webPassword());
    const QString e = ensureEngine(c);
    if (!e.isEmpty())
        return fail(err, e);
    // The sandboxed engine writes only where it was allowed to: a new folder restarts it.
    if (!folderAllowed(cfg, folder) && engineUnitActive()) {
        allowFolder(cfg, folder);
        QString se = updateSandbox(cfg);
        if (se.isEmpty())
            se = restartEngineUnit();
        if (se.isEmpty())
            se = ensureEngine(c);
        if (!se.isEmpty())
            return fail(err, se);
    }
    const Reply r = c.wait([&](Client::Done d) { c.add(urls, files, folder, start, d); });
    if (!r.ok())
        return fail(err, r.error);
    if (r.body.trimmed().startsWith("Fails"))
        return fail(err, "the engine didn't take it (already added, or not a torrent)");
    out << "added " << (urls.size() + files.size()) << " to " << folder << '\n';
    return 0;
}

static int cmdList(bool asJson, QTextStream &out, QTextStream &err)
{
    Config cfg;
    Client c(cfg.number("webui_port"), webPassword());
    const Reply r = c.wait([&c](Client::Done d) { c.get("torrents/info", {{"sort", "added_on"}, {"reverse", "true"}}, d); }, 8000);
    if (!r.ok()) {
        if (asJson) {
            json(out, {{"running", false}, {"torrents", QJsonArray()}});
            return 0;
        }
        return fail(err, r.error);
    }
    const QJsonArray list = QJsonDocument::fromJson(r.body).array();
    if (asJson) {
        QJsonArray slim;
        for (const QJsonValue &v : list) {
            const QJsonObject t = v.toObject();
            slim.append(QJsonObject{{"hash", t.value("hash")}, {"name", t.value("name")},
                                    {"state", t.value("state")}, {"progress", t.value("progress")},
                                    {"size", t.value("size")}, {"dlspeed", t.value("dlspeed")},
                                    {"upspeed", t.value("upspeed")}});
        }
        json(out, {{"running", true}, {"torrents", slim}});
        return 0;
    }
    for (const QJsonValue &v : list) {
        const QJsonObject t = v.toObject();
        out << QString::number(t.value("progress").toDouble() * 100, 'f', 1).rightJustified(6)
            << "%  " << t.value("state").toString().leftJustified(12) << ' '
            << t.value("name").toString() << '\n';
    }
    return 0;
}

int runCli(const QStringList &argsIn, QTextStream &out, QTextStream &err)
{
    QStringList args = argsIn;
    const bool asJson = args.removeAll("--json") > 0;
    const QString cmd = args.value(0);

    if (cmd == "--version" || cmd == "-V") {
        out << "uqbit " << UQBIT_VERSION << '\n';
        return 0;
    }
    if (cmd == "--help" || cmd == "-h") {
        out << kUsage;
        return 0;
    }
    if (cmd == "config")
        return cmdConfig(args, asJson, out, err);
    if (cmd == "default")
        return cmdDefault(args, asJson, out, err);
    if (cmd == "engine")
        return cmdEngine(args, asJson, out, err);
    if (cmd == "add")
        return cmdAdd(args, out, err);
    if (cmd == "list" && args.size() == 1)
        return cmdList(asJson, out, err);
    if (cmd == "status" && args.size() == 1)
        return cmdStatus(asJson, out, err);
    if ((cmd == "pause-all" || cmd == "resume-all") && args.size() == 1)
        return cmdAll(cmd == "resume-all", out, err);
    if (cmd == "show" && args.size() == 1)
        return cmdShow(err);
    if (cmd == "blocklist")
        return cmdBlocklist(args, asJson, out, err);
    if (cmd == "notify-finished" && args.size() >= 2) {
        notifyFinished(args.at(1), args.value(2));
        return 0;
    }
    return fail(err, "unknown command; see uqbit --help", 2);
}

} // namespace uqbit

#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>

namespace uqbit {

const QList<Key> &keys()
{
    static const QList<Key> all = {
        {"download_folder", "text", QStringLiteral("~/Downloads"), {},
         QStringLiteral("^~?/[^\\x00-\\x1f]{0,511}$"), 0, 0,
         "Where new torrents save their files."},
        {"ask_before_adding", "bool", true, {}, {}, 0, 0,
         "Show the torrent and its folder before adding it."},
        {"start_added", "bool", true, {}, {}, 0, 0,
         "Start downloading a torrent as soon as it is added."},
        {"notify_finished", "bool", true, {}, {}, 0, 0,
         "Show a notification when a download finishes."},
        {"keep_running", "bool", true, {}, {}, 0, 0,
         "Keep downloading and seeding after the window closes."},
        {"start_on_login", "bool", false, {}, {}, 0, 0,
         "Start the torrent engine when you log in."},
        {"launch_on_login", "bool", false, {}, {}, 0, 0,
         "Open uQbit when you log in (an XDG autostart entry, which uQ's Startup page lists)."},
        {"start_in_tray", "bool", false, {}, {}, 0, 0,
         "When uQbit opens at login, keep its window closed and wait in the tray."},
        {"close_to_tray", "bool", false, {}, {}, 0, 0,
         "Closing the window keeps uQbit in the tray instead of quitting it."},
        {"minimize_to_tray", "bool", false, {}, {}, 0, 0,
         "Minimising the window hides it to the tray."},
        {"download_limit", "int", 0, {}, {}, 0, 1000000, "Download limit in KiB/s (0: none)."},
        {"upload_limit", "int", 0, {}, {}, 0, 1000000, "Upload limit in KiB/s (0: none)."},
        {"seed_ratio", "int", 0, {}, {}, 0, 100,
         "Stop seeding a torrent once it has uploaded this many times its size (0: never; when_seeded says what happens)."},
        {"incomplete_folder", "text", QString(), {}, QStringLiteral("^(~?/[^\\x00-\\x1f]{0,511})?$"), 0, 0,
         "Keep unfinished downloads here and move them to the download folder when they finish (empty: off)."},
        {"watch_folder", "text", QString(), {}, QStringLiteral("^(~?/[^\\x00-\\x1f]{0,511})?$"), 0, 0,
         "Add .torrent files saved to this folder by themselves, then delete them there (empty: off)."},
        {"content_layout", "choice", QStringLiteral("original"),
         {QStringLiteral("original"), QStringLiteral("subfolder"), QStringLiteral("no_subfolder")}, {}, 0, 0,
         "How a torrent's files land in the folder: as the torrent has them (original), always in a folder of their own (subfolder), or never in one (no_subfolder)."},
        {"incomplete_extension", "bool", false, {}, {}, 0, 0,
         "Add .!qB to files that haven't finished downloading."},
        {"preallocate", "bool", false, {}, {}, 0, 0,
         "Reserve the disk space for a torrent's files when it's added."},
        {"max_active_downloads", "int", 3, {}, {}, 0, 999,
         "Download at most this many torrents at once; the rest wait their turn (0: no limit)."},
        {"max_active_uploads", "int", 3, {}, {}, 0, 999,
         "Seed at most this many torrents at once (0: no limit)."},
        {"max_active_torrents", "int", 5, {}, {}, 0, 999,
         "At most this many torrents downloading or seeding at once (0: no limit)."},
        {"seed_time", "int", 0, {}, {}, 0, 525600,
         "Stop seeding a torrent after this many minutes (0: never)."},
        {"when_seeded", "choice", QStringLiteral("stop"),
         {QStringLiteral("stop"), QStringLiteral("remove")}, {}, 0, 0,
         "What happens when a torrent reaches seed_ratio or seed_time: stop it, or remove it from the list (its files stay)."},
        {"tracker_lists", "text", QString(), {},
         QStringLiteral("^(https://[^\\s\\x00-\\x1f]{1,500}( https://[^\\s\\x00-\\x1f]{1,500}){0,9})?$"), 0, 0,
         "Lists of public trackers to add to new public torrents: up to 10 https:// links, separated by spaces (empty: none). Refreshed daily."},
        {"import_plugins", "bool", true, {}, {}, 0, 0,
         "Use the search plugins in qBittorrent's plugin folder too (what qBittorrent or qbit-plugin-dl installed there)."},
        {"webui_port", "int", 17845, {}, {}, 1024, 65535,
         "The local port uQbit talks to its engine on (127.0.0.1 only)."},
        {"vpn_only", "bool", false, {}, {}, 0, 0,
         "Send torrent traffic only through your VPN, and stop it while the VPN is down."},
        {"vpn_interface", "text", QString(), {}, QStringLiteral("^[A-Za-z0-9_.:@-]{0,15}$"), 0, 0,
         "The VPN's network interface (empty: the one that is up)."},
        {"encryption", "choice", QStringLiteral("require"),
         {QStringLiteral("require"), QStringLiteral("prefer"), QStringLiteral("allow")}, {}, 0, 0,
         "Connect only to peers that encrypt (require), prefer encryption, or allow either."},
        {"anonymous_mode", "bool", true, {}, {}, 0, 0,
         "Don't tell peers and trackers which client and version you run, or your local port."},
        {"blocklist", "bool", true, {}, {}, 0, 0,
         "Block peers on a public list of known monitoring and anti-piracy addresses."},
        {"blocklist_url", "text", QString::fromLatin1(kDefaultBlocklist), {},
         QStringLiteral("^https://[^\\s\\x00-\\x1f]{1,500}$"), 0, 0,
         "Where the blocklist comes from (p2p, dat or p2b, optionally gzipped)."},
    };
    return all;
}

const Key *findKey(const QString &name)
{
    for (const Key &k : keys())
        if (k.name == name)
            return &k;
    return nullptr;
}

static QString xdg(const char *var, const QString &fallback)
{
    const QString v = qEnvironmentVariable(var);
    return QDir::isAbsolutePath(v) ? v : QDir::homePath() + fallback;
}

QString configPath()
{
    const QString env = qEnvironmentVariable("UQBIT_CONFIG");
    if (!env.isEmpty())
        return env;
    return xdg("XDG_CONFIG_HOME", "/.config") + "/uqbit/config.json";
}

QString stateDir() { return xdg("XDG_STATE_HOME", "/.local/state") + "/uqbit"; }

QString profileDir()
{
    const QString env = qEnvironmentVariable("UQBIT_PROFILE");
    if (QDir::isAbsolutePath(env))
        return env;
    return xdg("XDG_DATA_HOME", "/.local/share") + "/uqbit";
}

QString expandHome(const QString &path)
{
    if (path == "~")
        return QDir::homePath();
    if (path.startsWith("~/"))
        return QDir::homePath() + path.mid(1);
    return path;
}

Config::Config(QString path) : m_path(std::move(path)) { load(); }

QVariant Config::parse(const Key &key, const QString &raw, QString *error)
{
    const QString v = raw.trimmed();
    if (key.type == "bool") {
        static const QStringList yes = {"true", "on", "yes", "1"};
        static const QStringList no = {"false", "off", "no", "0"};
        if (yes.contains(v.toLower()))
            return true;
        if (no.contains(v.toLower()))
            return false;
        *error = QStringLiteral("%1 is on or off (true/false), not \"%2\"").arg(key.name, raw);
        return {};
    }
    if (key.type == "choice") {
        if (key.choices.contains(v))
            return v;
        *error = QStringLiteral("%1 is one of %2").arg(key.name, key.choices.join(", "));
        return {};
    }
    if (key.type == "int") {
        bool ok = false;
        const int n = v.toInt(&ok);
        if (ok && QRegularExpression("^[0-9]{1,7}$").match(v).hasMatch() && n >= key.min
            && n <= key.max)
            return n;
        *error = QStringLiteral("%1 is a whole number from %2 to %3, not \"%4\"")
                     .arg(key.name).arg(key.min).arg(key.max).arg(raw);
        return {};
    }
    if (!QRegularExpression(key.pattern).match(v).hasMatch()
        || QRegularExpression("(^|/)\\.\\.(/|$)").match(v).hasMatch()) {
        *error = QStringLiteral("\"%1\" is not a valid %2").arg(raw, key.name);
        return {};
    }
    return v;
}

void Config::load()
{
    m_values = {};
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 64 * 1024)
        return;
    const QJsonObject in = QJsonDocument::fromJson(f.readAll()).object();
    for (const Key &k : keys()) {
        const QJsonValue v = in.value(k.name);
        QString raw;
        if (v.isBool())
            raw = v.toBool() ? "true" : "false";
        else if (v.isDouble())
            raw = QString::number(qint64(v.toDouble()));
        else if (v.isString())
            raw = v.toString();
        else
            continue;
        QString err;
        const QVariant ok = parse(k, raw, &err);
        if (ok.isValid())
            m_values.insert(k.name, QJsonValue::fromVariant(ok));
    }
}

QVariant Config::value(const QString &key) const
{
    const Key *k = findKey(key);
    if (!k)
        return {};
    const QJsonValue v = m_values.value(key);
    if (v.isUndefined())
        return k->fallback;
    return k->type == "int" ? QVariant(v.toInt()) : v.toVariant();
}

QString Config::save() const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly))
        return QStringLiteral("can't write %1: %2").arg(m_path, f.errorString());
    f.write(QJsonDocument(m_values).toJson(QJsonDocument::Indented));
    if (!f.commit())
        return QStringLiteral("can't write %1: %2").arg(m_path, f.errorString());
    return {};
}

QString Config::set(const QString &key, const QString &raw)
{
    const Key *k = findKey(key);
    if (!k)
        return QStringLiteral("unknown setting \"%1\"").arg(key);
    load();
    QString err;
    const QVariant v = parse(*k, raw, &err);
    if (!v.isValid())
        return err;
    m_values.insert(key, QJsonValue::fromVariant(v));
    return save();
}

QString Config::reset(const QString &key)
{
    if (!findKey(key))
        return QStringLiteral("unknown setting \"%1\"").arg(key);
    load();
    m_values.remove(key);
    return save();
}

QJsonObject Config::describe() const
{
    QJsonArray list;
    for (const Key &k : keys()) {
        QJsonObject o{{"key", k.name},
                      {"type", k.type},
                      {"default", QJsonValue::fromVariant(k.fallback)},
                      {"value", QJsonValue::fromVariant(value(k.name))},
                      {"set", m_values.contains(k.name)},
                      {"help", k.help}};
        if (!k.choices.isEmpty())
            o.insert("choices", QJsonArray::fromStringList(k.choices));
        if (k.type == "int") {
            o.insert("min", k.min);
            o.insert("max", k.max);
        }
        list.append(o);
    }
    return {{"settings", list}, {"path", m_path}};
}

} // namespace uqbit

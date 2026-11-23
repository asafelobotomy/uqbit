#include "plugins.h"

#include "client.h"
#include "config.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

namespace uqbit {

namespace {
constexpr qint64 kMaxPlugin = 1 << 20; // a plugin is a few KB; anything this big isn't one
constexpr int kMaxPlugins = 200;

QString dataHome()
{
    const QString x = qEnvironmentVariable("XDG_DATA_HOME");
    return x.startsWith('/') ? x : QDir::homePath() + "/.local/share";
}

QString canonical(const QString &dir)
{
    const QString c = QFileInfo(dir).canonicalFilePath();
    return c.isEmpty() ? QDir::cleanPath(dir) : c;
}

QString stagingDir() { return profileDir() + "/plugin-import"; }

bool saveImported(const QJsonObject &o)
{
    QDir().mkpath(stateDir());
    QSaveFile f(importedPluginsPath());
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return f.commit();
}

QString sha256(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > kMaxPlugin)
        return {};
    return QString::fromLatin1(
        QCryptographicHash::hash(f.read(kMaxPlugin), QCryptographicHash::Sha256).toHex());
}
} // namespace

QStringList qbtPluginDirs()
{
    const QString data = dataHome();
    return {data + "/qBittorrent/nova3/engines",
            QDir::homePath() + "/.var/app/org.qbittorrent.qBittorrent/data/qBittorrent/nova3/engines",
            data + "/data/qBittorrent/nova3/engines"};
}

QString enginePluginDir() { return profileDir() + "/qBittorrent/data/nova3/engines"; }

bool validPluginFile(const QString &fileName)
{
    static const QRegularExpression ok(QStringLiteral("^[A-Za-z][A-Za-z0-9_]{0,63}\\.py$"));
    static const QSet<QString> reserved{"helpers", "novaprinter", "nova2",   "nova2dl",
                                        "socks",   "sgmllib3",    "template"};
    return ok.match(fileName).hasMatch() && !reserved.contains(fileName.chopped(3).toLower());
}

QList<QbtPlugin> qbtPlugins(const QStringList &dirs)
{
    QList<QbtPlugin> out;
    QSet<QString> seen;
    const QString own = canonical(enginePluginDir());
    for (const QString &dir : dirs) {
        if (!QFileInfo(dir).isDir() || canonical(dir) == own)
            continue;
        const QFileInfoList files =
            QDir(dir).entryInfoList({"*.py"}, QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo &f : files) {
            const QString name = f.completeBaseName();
            if (!validPluginFile(f.fileName()) || seen.contains(name) || f.size() > kMaxPlugin
                || f.size() == 0 || out.size() >= kMaxPlugins)
                continue;
            seen.insert(name);
            out << QbtPlugin{name, f.absoluteFilePath(),
                             QString::number(f.size()) + ':'
                                 + QString::number(f.lastModified().toMSecsSinceEpoch())};
        }
    }
    return out;
}

static void importInto(Client *client, const QSet<QString> &installed,
                       std::function<void(const ImportResult &)> done, const QStringList &dirs);

QString importedPluginsPath() { return stateDir() + "/imported-plugins.json"; }

QJsonObject importedPlugins()
{
    QFile f(importedPluginsPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.read(1 << 20)).object();
}

void importPlugins(Client *client, std::function<void(const ImportResult &)> done,
                   const QStringList &dirs)
{
    // What the engine has: a plugin of the same name that uQbit didn't bring in is the user's
    // own (installed in uQbit), and stays as it is.
    client->get("search/plugins", {}, [client, done, dirs](const Reply &r) {
        if (!r.ok()) {
            done(ImportResult{{}, {}, r.error});
            return;
        }
        QSet<QString> installed;
        for (const QJsonValue &v : QJsonDocument::fromJson(r.body).array())
            installed.insert(v.toObject().value("name").toString());
        importInto(client, installed, done, dirs);
    });
}

static void importInto(Client *client, const QSet<QString> &installed,
                       std::function<void(const ImportResult &)> done, const QStringList &dirs)
{
    QJsonObject state = importedPlugins();
    const QList<QbtPlugin> found = qbtPlugins(dirs);
    ImportResult result;
    QStringList sources;
    QJsonObject next;
    bool touched = false; // only the stamps changed: save, but nothing for the engine
    QDir().mkpath(stagingDir());
    for (const QbtPlugin &p : found) {
        const QJsonObject was = state.value(p.name).toObject();
        if (was.isEmpty() && installed.contains(p.name))
            continue;
        if (was.value("stamp").toString() == p.stamp) {
            next.insert(p.name, was);
            continue;
        }
        const QString sha = sha256(p.path);
        if (sha.isEmpty())
            continue;
        QJsonObject now{{"sha256", sha}, {"stamp", p.stamp}, {"from", p.path}};
        next.insert(p.name, now);
        touched = true;
        if (was.value("sha256").toString() == sha)
            continue;
        // The sandboxed engine can't read the user's other folders: give it a copy in its
        // profile. The copy is named like the plugin, since that is how the engine names it.
        const QString copy = stagingDir() + '/' + p.name + ".py";
        QFile::remove(copy);
        if (!QFile::copy(p.path, copy)) {
            next.remove(p.name);
            result.error = QStringLiteral("couldn't copy %1").arg(p.path);
            continue;
        }
        sources << copy;
        result.added << p.name;
    }
    for (auto it = state.begin(); it != state.end(); ++it)
        if (!next.contains(it.key()))
            result.removed << it.key();
    if (sources.isEmpty() && result.removed.isEmpty()) {
        if (touched)
            saveImported(next);
        done(result);
        return;
    }

    auto finish = [client, next, result, sources, done](const Reply &r) mutable {
        for (const QString &s : sources)
            QFile::remove(s);
        if (!r.ok()) {
            // Try again next time: keep the old record for what didn't go through.
            result.error = r.error.isEmpty() ? QStringLiteral("the engine said no") : r.error;
            result.added.clear();
            result.removed.clear();
            done(result);
            return;
        }
        saveImported(next);
        done(result);
    };
    auto install = [client, sources, finish](const Reply &r) mutable {
        if (!r.ok() || sources.isEmpty()) {
            finish(r);
            return;
        }
        QUrlQuery form;
        form.addQueryItem("sources", sources.join('|'));
        client->post("search/installPlugin", form, finish);
    };
    if (result.removed.isEmpty()) {
        install(Reply{200, {}, {}});
        return;
    }
    QUrlQuery form;
    form.addQueryItem("names", result.removed.join('|'));
    client->post("search/uninstallPlugin", form, install);
}

} // namespace uqbit

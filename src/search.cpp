// The App's search: qBittorrent's search plugins, run by the engine (they need Python), and
// the plugins themselves (install, turn on or off, update, remove).
#include "app.h"
#include "plugins.h"

#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>

namespace uqbit {

namespace {
struct Official {
    const char *name, *title, *about;
};
// qBittorrent's own plugins (github.com/qbittorrent/search-plugins, nova3/engines).
constexpr Official kOfficial[] = {
    {"eztv", "EZTV", "TV shows"},
    {"jackett", "Jackett", "Your own Jackett server (set it up first)"},
    {"limetorrents", "LimeTorrents", "General"},
    {"piratebay", "The Pirate Bay", "General"},
    {"solidtorrents", "Solid Torrents", "General"},
    {"torlock", "TorLock", "General, verified torrents"},
    {"torrentproject", "TorrentProject", "General"},
    {"torrentscsv", "torrents-csv", "General, an open database"},
};
constexpr auto kOfficialBase =
    "https://raw.githubusercontent.com/qbittorrent/search-plugins/master/nova3/engines/";
constexpr int kMaxResults = 500;
} // namespace

static bool havePython()
{
    return !QStandardPaths::findExecutable("python3").isEmpty()
           || !QStandardPaths::findExecutable("python").isEmpty();
}

QVariantList App::officialPlugins() const
{
    QVariantList out;
    for (const Official &o : kOfficial) {
        const QString name = QString::fromLatin1(o.name);
        const bool installed = std::any_of(m_plugins.begin(), m_plugins.end(),
                                           [&](const QVariant &p) {
                                               return p.toMap().value("name") == name;
                                           });
        if (!installed)
            out << QVariantMap{{"name", name},
                               {"title", QString::fromLatin1(o.title)},
                               {"about", QString::fromLatin1(o.about)},
                               {"url", QString::fromLatin1(kOfficialBase) + name + ".py"}};
    }
    return out;
}

void App::search(const QString &patternIn, const QString &category)
{
    const QString pattern = patternIn.simplified();
    if (!m_client || pattern.isEmpty())
        return;
    if (!havePython()) {
        m_searchState = "unavailable";
        m_searchError = tr("Searching needs Python 3 (the python package).");
        emit searchChanged();
        return;
    }
    stopSearch();
    refreshPlugins();
    static const QStringList cats{"all",   "movies",   "tv",       "music", "games",
                                  "anime", "software", "pictures", "books"};
    QUrlQuery form;
    form.addQueryItem("pattern", pattern.left(200));
    form.addQueryItem("plugins", "enabled");
    form.addQueryItem("category", cats.contains(category) ? category : "all");
    m_results.clear();
    m_searchTotal = 0;
    m_searchError.clear();
    m_searchState = "running";
    emit searchChanged();
    m_client->post("search/start", form, [this](const Reply &r) {
        if (!r.ok()) {
            m_searchState = "error";
            m_searchError = r.status == 409
                                ? tr("Turn on a search plugin first (Plugins).")
                                : r.error;
            emit searchChanged();
            return;
        }
        m_searchId = QJsonDocument::fromJson(r.body).object().value("id").toInt(-1);
        m_searchTimer.start();
        pollSearch();
    });
}

void App::pollSearch()
{
    if (!m_client || m_searchId < 0) {
        m_searchTimer.stop();
        return;
    }
    QUrlQuery q;
    q.addQueryItem("id", QString::number(m_searchId));
    q.addQueryItem("limit", "200");
    q.addQueryItem("offset", QString::number(m_searchTotal));
    const int id = m_searchId;
    m_client->get("search/results", q, [this, id](const Reply &r) {
        if (id != m_searchId)
            return;
        if (!r.ok()) {
            m_searchTimer.stop();
            m_searchState = "error";
            m_searchError = r.error;
            emit searchChanged();
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(r.body).object();
        const QJsonArray list = o.value("results").toArray();
        m_searchTotal += int(list.size());
        for (const QJsonValue &v : list) {
            if (m_results.size() >= kMaxResults)
                break;
            const QJsonObject x = v.toObject();
            const QString url = x.value("fileUrl").toString();
            if (url.isEmpty())
                continue;
            m_results << QVariantMap{{"name", x.value("fileName").toString()},
                                     {"url", url},
                                     {"size", x.value("fileSize").toDouble()},
                                     {"seeds", x.value("nbSeeders").toInt()},
                                     {"leechers", x.value("nbLeechers").toInt()},
                                     {"site", QUrl(x.value("siteUrl").toString()).host()},
                                     {"engine", x.value("engineName").toString()},
                                     {"page", x.value("descrLink").toString()}};
        }
        std::stable_sort(m_results.begin(), m_results.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value("seeds").toInt() > b.toMap().value("seeds").toInt();
        });
        if (o.value("status").toString() == "Stopped") {
            m_searchTimer.stop();
            m_searchState = "done";
            QUrlQuery del;
            del.addQueryItem("id", QString::number(m_searchId));
            m_client->post("search/delete", del, [](const Reply &) {});
            m_searchId = -1;
        }
        emit searchChanged();
    });
}

void App::stopSearch()
{
    m_searchTimer.stop();
    if (m_client && m_searchId >= 0) {
        QUrlQuery form;
        form.addQueryItem("id", QString::number(m_searchId));
        m_client->post("search/stop", form, [this, form](const Reply &) {
            m_client->post("search/delete", form, [](const Reply &) {});
        });
    }
    m_searchId = -1;
    if (m_searchState == "running") {
        m_searchState = "done";
        emit searchChanged();
    }
}

void App::clearSearch()
{
    stopSearch();
    m_results.clear();
    m_searchTotal = 0;
    m_searchState = "idle";
    m_searchError.clear();
    emit searchChanged();
}

void App::addResult(int index)
{
    if (!m_client || index < 0 || index >= m_results.size())
        return;
    const QVariantMap r = m_results.at(index).toMap();
    const QString url = r.value("url").toString();
    if (url.startsWith("magnet:")) {
        openInputs({url});
        return;
    }
    if (!url.startsWith("https://") && !url.startsWith("http://")) {
        emit notice(tr("That result has no link uQbit can open"));
        return;
    }
    // Often a page, not a .torrent: the plugin knows how to get the torrent from it, so the
    // engine asks the plugin and adds what it gets (to the download folder).
    QUrlQuery form;
    form.addQueryItem("torrentUrl", url);
    form.addQueryItem("pluginName", r.value("engine").toString());
    m_client->post("search/downloadTorrent", form, [this, url, r](const Reply &rep) {
        if (rep.status == 404) { // qBittorrent 4: no downloadTorrent; hope it is a .torrent
            openInputs({url});
            return;
        }
        if (!rep.ok()) {
            emit notice(tr("Couldn't add: %1").arg(rep.error));
            return;
        }
        emit notice(tr("Getting the torrent from %1…").arg(r.value("site").toString()));
        QTimer::singleShot(1500, this, &App::poll);
    });
}

void App::openPage(int index)
{
    if (index < 0 || index >= m_results.size())
        return;
    const QUrl page(m_results.at(index).toMap().value("page").toString());
    if (page.scheme() == "https" || page.scheme() == "http")
        QDesktopServices::openUrl(page);
}

void App::refreshPlugins()
{
    if (!m_client)
        return;
    m_client->get("search/plugins", {}, [this](const Reply &r) {
        if (!r.ok())
            return;
        QVariantList list;
        const QJsonObject imported = importedPlugins();
        for (const QJsonValue &v : QJsonDocument::fromJson(r.body).array()) {
            const QJsonObject p = v.toObject();
            QStringList cats;
            for (const QJsonValue &c : p.value("supportedCategories").toArray())
                cats << (c.isObject() ? c.toObject().value("name").toString() : c.toString());
            list << QVariantMap{{"name", p.value("name").toString()},
                                {"fullName", p.value("fullName").toString()},
                                {"version", p.value("version").toString()},
                                {"enabled", p.value("enabled").toBool()},
                                {"url", p.value("url").toString()},
                                {"imported", imported.contains(p.value("name").toString())},
                                {"categories", cats}};
        }
        std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value("fullName").toString().compare(
                       b.toMap().value("fullName").toString(), Qt::CaseInsensitive) < 0;
        });
        m_plugins = list;
        emit pluginsChanged();
    });
}

void App::pluginCall(const QString &path, const QUrlQuery &form, const QString &okText)
{
    if (!m_client)
        return;
    m_pluginsBusy = true;
    emit pluginsChanged();
    m_client->post(path, form, [this, okText](const Reply &r) {
        if (!r.ok()) {
            m_pluginsBusy = false;
            emit pluginsChanged();
            emit notice(tr("The plugin change failed: %1").arg(r.error));
            return;
        }
        // The engine installs and updates in the background: look again in a moment.
        for (int ms : {700, 2500, 6000})
            QTimer::singleShot(ms, this, [this, ms] {
                if (ms == 6000) {
                    m_pluginsBusy = false;
                }
                refreshPlugins();
            });
        if (!okText.isEmpty())
            emit notice(okText);
    });
}

void App::installPlugin(const QString &urlOrName)
{
    QString source = urlOrName.trimmed();
    for (const Official &o : kOfficial)
        if (source == QLatin1String(o.name))
            source = QString::fromLatin1(kOfficialBase) + source + ".py";
    const QUrl url(source);
    if (url.scheme() != "https" || !url.path().endsWith(".py") || source.contains('|')) {
        emit notice(tr("A search plugin is an https:// link to a .py file"));
        return;
    }
    QUrlQuery form;
    form.addQueryItem("sources", source);
    pluginCall("search/installPlugin", form, tr("Installing the plugin…"));
}

void App::uninstallPlugin(const QString &name)
{
    if (!QRegularExpression("^[A-Za-z0-9_]{1,64}$").match(name).hasMatch())
        return;
    QUrlQuery form;
    form.addQueryItem("names", name);
    pluginCall("search/uninstallPlugin", form, tr("Removed"));
}

void App::enablePlugin(const QString &name, bool on)
{
    if (!QRegularExpression("^[A-Za-z0-9_]{1,64}$").match(name).hasMatch())
        return;
    QUrlQuery form;
    form.addQueryItem("names", name);
    form.addQueryItem("enable", on ? "true" : "false");
    pluginCall("search/enablePlugin", form, QString());
}

void App::importQbtPlugins()
{
    if (!m_client || m_status != "ready" || m_importing || !m_config.boolean("import_plugins"))
        return;
    m_importing = true;
    uqbit::importPlugins(m_client, [this](const ImportResult &r) {
        m_importing = false;
        if (r.added.isEmpty() && r.removed.isEmpty())
            return;
        // The engine loads new plugins in the background: look again in a moment.
        for (int ms : {700, 2500, 6000})
            QTimer::singleShot(ms, this, &App::refreshPlugins);
        if (!r.added.isEmpty())
            emit notice(tr("Brought in %n search plugin(s) from qBittorrent", "", int(r.added.size())));
    });
}

void App::updatePlugins()
{
    pluginCall("search/updatePlugins", {}, tr("Updating the plugins…"));
}

} // namespace uqbit

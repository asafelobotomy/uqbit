#include "app.h"
#include "defaultapp.h"
#include "engine.h"
#include "torrentfile.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

namespace uqbit {

static App *s_instance = nullptr;

App *App::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

App::App(const QStringList &initial, QObject *parent) : QObject(parent), m_initial(initial)
{
    s_instance = this;
    loadSettings();
    updateDefault();

    // The settings file is replaced on save, so watch its folder as well.
    const QString cfg = m_config.path();
    QDir().mkpath(QFileInfo(cfg).absolutePath());
    m_configWatch.addPath(QFileInfo(cfg).absolutePath());
    if (QFileInfo::exists(cfg))
        m_configWatch.addPath(cfg);
    auto reload = [this, cfg] {
        if (QFileInfo::exists(cfg) && !m_configWatch.files().contains(cfg))
            m_configWatch.addPath(cfg);
        const QVariantMap before = m_settings;
        loadSettings();
        if (before == m_settings)
            return;
        if (before.value("webui_port") != m_settings.value("webui_port")) {
            connectEngine();
            return;
        }
        applyPreferences();
    };
    connect(&m_configWatch, &QFileSystemWatcher::fileChanged, this, reload);
    connect(&m_configWatch, &QFileSystemWatcher::directoryChanged, this, reload);

    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, &App::poll);
    m_detailsTimer.setInterval(2000);
    connect(&m_detailsTimer, &QTimer::timeout, this, &App::refreshDetails);

    // The engine runs on; it stops with the window only when the settings say so.
    if (auto *core = QCoreApplication::instance())
        connect(core, &QCoreApplication::aboutToQuit, this, [this] {
            if (m_client && !m_config.boolean("keep_running") && m_status == "ready")
                m_client->wait([this](Client::Done d) { m_client->shutdown(d); }, 3000);
        });

    QTimer::singleShot(0, this, &App::connectEngine);
}

App::~App() = default;

void App::loadSettings()
{
    m_config.load();
    QVariantMap s;
    for (const Key &k : keys())
        s.insert(k.name, m_config.value(k.name));
    if (s != m_settings) {
        m_settings = s;
        emit settingsChanged();
    }
}

void App::setStatus(const QString &status, const QString &text)
{
    if (status == m_status && text == m_statusText)
        return;
    m_status = status;
    m_statusText = text;
    emit statusChanged();
}

void App::connectEngine()
{
    m_poll.stop();
    m_rid = 0;
    m_tries = 0;
    m_torrents.clear();
    if (engineBinary().isEmpty()) {
        setStatus("missing", tr("qBittorrent's engine (qbittorrent-nox) isn't installed."));
        return;
    }
    QString err;
    const QString pw = webPassword(&err);
    if (pw.isEmpty()) {
        setStatus("error", err);
        return;
    }
    delete m_client;
    m_client = new Client(m_config.number("webui_port"), pw, this);
    setStatus("starting", tr("Starting the torrent engine…"));
    poll();
}

void App::poll()
{
    if (!m_client || m_polling)
        return;
    m_polling = true;
    m_client->maindata(m_rid, [this](const Reply &r) {
        m_polling = false;
        if (r.ok()) {
            const bool first = m_status != "ready";
            m_rid = m_torrents.apply(QJsonDocument::fromJson(r.body).object());
            if (first) {
                m_tries = 0;
                setStatus("ready", QString());
                m_client->version([this](const Reply &v) {
                    if (!v.ok())
                        return;
                    m_engineVersion = QString::fromUtf8(v.body).trimmed();
                    emit statusChanged();
                });
                applyPreferences();
                if (!m_initial.isEmpty()) {
                    const QStringList in = m_initial;
                    m_initial.clear();
                    openInputs(in);
                }
            }
            if (!m_poll.isActive())
                m_poll.start();
            return;
        }
        // Not answering: start it once, then keep trying for a while.
        if (m_status == "ready") {
            m_torrents.clear();
            m_rid = 0;
            setStatus("starting", tr("The torrent engine stopped. Starting it again…"));
            m_startedEngine = false;
        }
        if (!m_startedEngine && r.status == 0) {
            m_startedEngine = true;
            const QString e = startEngine(QCoreApplication::applicationFilePath());
            if (!e.isEmpty()) {
                setStatus("error", e);
                return;
            }
        }
        if (++m_tries > 60) {
            m_poll.stop();
            setStatus("error", r.error.isEmpty() ? tr("The torrent engine didn't start.") : r.error);
            return;
        }
        m_poll.stop();
        QTimer::singleShot(r.status == 0 ? 500 : 2000, this, &App::poll);
    });
}

void App::retry()
{
    m_startedEngine = false;
    connectEngine();
}

void App::applyPreferences()
{
    if (!m_client)
        return;
    m_client->setPreferences(preferencesFor(m_config, QCoreApplication::applicationFilePath()),
                             [this](const Reply &r) { done(r, tr("Couldn't apply the settings")); });
}

void App::done(const Reply &r, const QString &failed)
{
    if (!r.ok())
        emit notice(failed + ": " + r.error);
    else
        poll();
}

void App::select(const QString &hash)
{
    if (hash == m_selected)
        return;
    m_selected = hash;
    m_details = {};
    emit selectedChanged();
    emit detailsChanged();
    if (hash.isEmpty()) {
        m_detailsTimer.stop();
        return;
    }
    refreshDetails();
    m_detailsTimer.start();
}

void App::refreshDetails()
{
    if (!m_client || m_selected.isEmpty())
        return;
    const QString hash = m_selected;
    auto store = [this, hash](const QString &key) {
        return [this, hash, key](const Reply &r) {
            if (hash != m_selected || !r.ok())
                return;
            const QJsonDocument d = QJsonDocument::fromJson(r.body);
            const QVariant v = d.isArray() ? QVariant(d.array().toVariantList())
                                           : QVariant(d.object().toVariantMap());
            if (m_details.value(key) == v)
                return;
            m_details.insert(key, v);
            emit detailsChanged();
        };
    };
    m_client->get("torrents/properties", {{"hash", hash}}, store("props"));
    m_client->get("torrents/files", {{"hash", hash}}, store("files"));
    m_client->get("torrents/trackers", {{"hash", hash}}, store("trackers"));
}

void App::setRunning(const QStringList &hashes, bool run)
{
    if (m_client && !hashes.isEmpty())
        m_client->startStop(hashes, run, [this, run](const Reply &r) {
            done(r, run ? tr("Couldn't start it") : tr("Couldn't stop it"));
        });
}

void App::remove(const QStringList &hashes, bool deleteFiles)
{
    if (!m_client || hashes.isEmpty())
        return;
    if (hashes.contains(m_selected))
        select(QString());
    m_client->remove(hashes, deleteFiles, [this, deleteFiles](const Reply &r) {
        if (r.ok())
            emit notice(deleteFiles ? tr("Removed, with its files") : tr("Removed; its files stay"));
        done(r, tr("Couldn't remove it"));
    });
}

void App::recheck(const QString &hash)
{
    if (m_client)
        m_client->hashesAction("recheck", {hash}, [this](const Reply &r) { done(r, tr("Couldn't check it")); });
}

void App::reannounce(const QString &hash)
{
    if (m_client)
        m_client->hashesAction("reannounce", {hash}, [this](const Reply &r) {
            if (r.ok())
                emit notice(tr("Asked the trackers for more peers"));
            done(r, tr("Couldn't reach the trackers"));
        });
}

void App::setFileWanted(int index, bool wanted)
{
    if (!m_client || m_selected.isEmpty())
        return;
    m_client->setFilePriority(m_selected, {index}, wanted ? 1 : 0, [this](const Reply &r) {
        if (!r.ok())
            emit notice(tr("Couldn't change that file: %1").arg(r.error));
        refreshDetails();
    });
}

void App::openFolder(const QString &hash)
{
    const QVariantMap t = m_torrents.get(hash);
    QString path = t.value("content_path").toString();
    if (!QFileInfo(path).isDir())
        path = t.value("save_path").toString();
    if (path.isEmpty() || !QFileInfo(path).exists()) {
        emit notice(tr("The folder isn't there yet"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void App::copyMagnet(const QString &hash)
{
    const QString magnet = m_torrents.get(hash).value("magnet_uri").toString();
    if (magnet.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(magnet);
    emit notice(tr("Magnet link copied"));
}

void App::updateDefault()
{
    const bool d = defaultStatus().value("default").toBool();
    if (d != m_isDefault) {
        m_isDefault = d;
        emit defaultChanged();
    }
}

void App::makeDefault()
{
    const QString e = uqbit::makeDefault();
    if (!e.isEmpty())
        emit notice(e);
    updateDefault();
}

void App::raise()
{
    if (m_window) {
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
    }
}

void App::quit() { QCoreApplication::quit(); }

} // namespace uqbit

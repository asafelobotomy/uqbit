// The App's privacy side: the VPN kill switch kept bound to the right interface, the
// blocklist kept fresh, the sandboxed engine given the folders torrents are added to.
#include "app.h"
#include "trackers.h"
#include "engine.h"
#include "privacy.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPointer>
#include <QThreadPool>

namespace uqbit {

void App::setProtection(const QVariantMap &p)
{
    if (p == m_protection)
        return;
    m_protection = p;
    emit protectionChanged();
}

void App::checkProtection()
{
    refreshTrackers();
    // Detection may ask NetworkManager (a process): off the window's thread.
    const Config config = m_config;
    QPointer<App> self(this);
    QThreadPool::globalInstance()->start([self, config] {
        const VpnState vpn = vpnState(config);
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, vpn] {
            if (!self)
                return;
            App *app = self.data();
            // Rebind when the VPN came back under another name (or vpn_only changed).
            if (app->m_client && app->m_status == "ready" && app->m_boundKnown
                && !app->m_binding && app->m_bound != vpn.interface) {
                const QString want = vpn.interface;
                app->m_binding = true;
                bindEngine(app->m_client, want, [self, want](const Reply &r) {
                    if (!self)
                        return;
                    self->m_binding = false;
                    if (r.ok()) {
                        self->m_bound = want;
                        self->checkProtection();
                    }
                });
            }
            const QJsonObject j = vpnJson(vpn, app->m_bound, app->m_boundKnown);
            QVariantMap p = j.toVariantMap();
            const QVariantMap v = p.value("vpn").toMap();
            for (auto it = v.begin(); it != v.end(); ++it)
                p.insert(it.key(), it.value());
            p.remove("vpn");
            p.insert("encryption", app->m_config.text("encryption"));
            p.insert("anonymous", app->m_config.boolean("anonymous_mode"));
            const QString list = blocklistPath();
            p.insert("blocklist",
                     QVariantMap{{"on", app->m_config.boolean("blocklist")},
                                 {"loaded", !list.isEmpty()},
                                 {"updated", list.isEmpty() ? QString()
                                                            : QFileInfo(list).lastModified()
                                                                  .toString(Qt::ISODate)},
                                 {"updating", app->m_blocklistUpdating},
                                 {"error", app->m_blocklistError}});
            app->setProtection(p);
        });
    });
}

void App::engineReady()
{
    applyPreferences();
    // What the engine is bound to: the kill switch is only "on" once it says so.
    m_client->preferences([this](const Reply &r) {
        if (!r.ok())
            return;
        m_bound = QJsonDocument::fromJson(r.body).object()
                      .value("current_network_interface").toString();
        m_boundKnown = true;
        checkProtection();
    });
    if (m_config.boolean("blocklist") && blocklistStale())
        updateBlocklist();
    refreshTrackers();
    refreshPlugins();
    importQbtPlugins();
    if (m_afterReady) {
        auto then = std::move(m_afterReady);
        m_afterReady = nullptr;
        then();
    }
    if (!m_initial.isEmpty()) {
        const QStringList in = m_initial;
        m_initial.clear();
        openInputs(in);
    }
}

void App::updateBlocklist()
{
    if (!m_client || m_blocklistUpdating)
        return;
    m_blocklistUpdating = true;
    m_blocklistError.clear();
    checkProtection();
    uqbit::updateBlocklist(m_client, m_config, [this](const QString &e) {
        m_blocklistUpdating = false;
        m_blocklistError = e;
        if (!e.isEmpty())
            emit notice(e);
        checkProtection();
    });
}

void App::refreshTrackers()
{
    // Daily, and only with lists set; the `config set` that adds a list fetches it at once.
    // A failed refresh keeps the last list and tries again in an hour, not every 5 s.
    if (!m_client || m_status != "ready" || m_trackersUpdating || trackerListUrls(m_config).isEmpty()
        || !trackersStale()
        || (m_trackersTried.isValid() && m_trackersTried.secsTo(QDateTime::currentDateTime()) < 3600))
        return;
    m_trackersUpdating = true;
    m_trackersTried = QDateTime::currentDateTime();
    uqbit::updateTrackerLists(m_client, m_config, true,
                              [this](const QString &, int) { m_trackersUpdating = false; });
}

QString App::setSetting(const QString &key, const QString &value)
{
    const QString e = m_config.set(key, value);
    if (!e.isEmpty())
        return e;
    // The file watcher picks it up (and applies it); this just doesn't wait for it.
    loadSettings();
    applyPreferences();
    checkProtection();
    if (key == "blocklist" && m_config.boolean("blocklist") && blocklistStale())
        updateBlocklist();
    if (key == "import_plugins")
        importQbtPlugins();
    return {};
}

bool App::makeWritable(const QString &folder, std::function<void()> then)
{
    if (folderAllowed(m_config, folder) || !engineUnitActive())
        return false;
    allowFolder(m_config, folder);
    const QString e = updateSandbox(m_config);
    if (!e.isEmpty()) {
        emit notice(tr("Couldn't let the engine save there: %1").arg(e));
        return false;
    }
    emit notice(tr("Letting the torrent engine save to %1…").arg(prettyFolder(folder)));
    m_afterReady = std::move(then);
    const QString re = restartEngineUnit();
    if (!re.isEmpty()) {
        m_afterReady = nullptr;
        emit notice(re);
        return false;
    }
    m_startedEngine = true; // systemd brings it back; don't start a second one
    m_boundKnown = false;
    m_torrents.clear();
    m_rid = 0;
    setStatus("starting", tr("Restarting the torrent engine…"));
    m_poll.stop();
    QTimer::singleShot(800, this, &App::poll);
    return true;
}

} // namespace uqbit

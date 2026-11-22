// The glue between the window, the engine and the settings. QML sees it as `App`.
#pragma once

#include "client.h"
#include "config.h"
#include "theme.h"
#include "torrents.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace uqbit {

class App : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(uqbit::TorrentModel *torrents READ torrents CONSTANT)
    Q_PROPERTY(uqtheme::Watcher *theme READ theme CONSTANT)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    /// "starting", "ready", "missing" (no qbittorrent-nox) or "error".
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString engineVersion READ engineVersion NOTIFY statusChanged)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY defaultChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    /// The torrent the side panel shows ("" for none), and what the engine says about it:
    /// {props: {...}, files: [...], trackers: [...]}.
    Q_PROPERTY(QString selected READ selected WRITE select NOTIFY selectedChanged)
    Q_PROPERTY(QVariantMap details READ details NOTIFY detailsChanged)
    /// What's waiting in the "add" sheet: [{input, kind: magnet|file|url, name, size, files,
    /// file_count, ok, error}].
    Q_PROPERTY(QVariantList pending READ pending NOTIFY pendingChanged)
    /// How private torrenting is right now: {required, interface, up, bound, protected,
    /// encryption, anonymous, blocklist: {on, loaded, updated, updating, error}}.
    Q_PROPERTY(QVariantMap protection READ protection NOTIFY protectionChanged)
    /// uQbit has an icon in the tray: closing or minimising the window can hide it there.
    Q_PROPERTY(bool trayShown READ trayShown NOTIFY trayChanged)
    /// Search through the engine's plugins. searchState: "idle", "running", "done", "error"
    /// or "unavailable" (no Python); results: [{name, url, size, seeds, leechers, site,
    /// engine, page}], best seeded first.
    Q_PROPERTY(QString searchState READ searchState NOTIFY searchChanged)
    Q_PROPERTY(QString searchError READ searchError NOTIFY searchChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
    Q_PROPERTY(int searchTotal READ searchTotal NOTIFY searchChanged)
    /// The installed search plugins: [{name, fullName, version, enabled, url, categories}].
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(bool pluginsBusy READ pluginsBusy NOTIFY pluginsChanged)
    /// qBittorrent's own plugins that aren't installed: [{name, title, url, about}].
    Q_PROPERTY(QVariantList officialPlugins READ officialPlugins NOTIFY pluginsChanged)

public:
    explicit App(const QStringList &initial, QObject *parent = nullptr);
    ~App() override;
    static App *create(QQmlEngine *, QJSEngine *);

    TorrentModel *torrents() { return &m_torrents; }
    uqtheme::Watcher *theme() { return &m_theme; }
    QVariantMap settings() const { return m_settings; }
    QString status() const { return m_status; }
    QString statusText() const { return m_statusText; }
    QString engineVersion() const { return m_engineVersion; }
    bool isDefault() const { return m_isDefault; }
    QString version() const { return QStringLiteral(UQBIT_VERSION); }
    QString selected() const { return m_selected; }
    void select(const QString &hash);
    QVariantMap details() const { return m_details; }
    QVariantList pending() const { return m_pending; }
    void setWindow(QQuickWindow *w) { m_window = w; }
    bool trayShown() const { return m_trayShown; }
    void setTrayShown(bool on)
    {
        if (on != m_trayShown) {
            m_trayShown = on;
            emit trayChanged();
        }
    }
    QVariantMap protection() const { return m_protection; }
    QString searchState() const { return m_searchState; }
    QString searchError() const { return m_searchError; }
    QVariantList searchResults() const { return m_results; }
    int searchTotal() const { return m_searchTotal; }
    QVariantList plugins() const { return m_plugins; }
    bool pluginsBusy() const { return m_pluginsBusy; }
    QVariantList officialPlugins() const;

    /// Magnet links, torrent URLs and .torrent files (paths or file:// URLs): into the add
    /// sheet, or straight to the engine when "ask before adding" is off.
    Q_INVOKABLE void openInputs(const QStringList &inputs);
    /// The magnet links / torrent URLs on the clipboard, through openInputs. Returns how many.
    Q_INVOKABLE int pasteLinks();
    /// Sends what's in the add sheet to `folder` (local path, "~/..." or file:// URL).
    Q_INVOKABLE void addPending(const QString &folder, bool start);
    Q_INVOKABLE void dropPending(int index);
    Q_INVOKABLE void clearPending();
    Q_INVOKABLE void setRunning(const QStringList &hashes, bool run);
    Q_INVOKABLE void remove(const QStringList &hashes, bool deleteFiles);
    Q_INVOKABLE void recheck(const QString &hash);
    Q_INVOKABLE void reannounce(const QString &hash);
    Q_INVOKABLE void setFileWanted(int index, bool wanted);
    Q_INVOKABLE void openFolder(const QString &hash);
    Q_INVOKABLE void copyMagnet(const QString &hash);
    Q_INVOKABLE void makeDefault();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void raise();
    Q_INVOKABLE void quit();
    /// A folder (path, "~/..." or file:// URL) shown the way the settings write it ("~/x").
    Q_INVOKABLE QString prettyFolder(const QString &folder) const;
    Q_INVOKABLE QString downloadFolder() const;
    /// Changes one setting (as `uqbit config set` does); returns an error or "".
    Q_INVOKABLE QString setSetting(const QString &key, const QString &value);
    Q_INVOKABLE void updateBlocklist();

    Q_INVOKABLE void search(const QString &pattern, const QString &category);
    Q_INVOKABLE void stopSearch();
    Q_INVOKABLE void clearSearch();
    /// Adds a search result (its link or magnet) the way a pasted link is added.
    Q_INVOKABLE void addResult(int index);
    Q_INVOKABLE void openPage(int index);
    Q_INVOKABLE void refreshPlugins();
    /// A plugin's .py URL (https only) or one of officialPlugins' names.
    Q_INVOKABLE void installPlugin(const QString &urlOrName);
    Q_INVOKABLE void uninstallPlugin(const QString &name);
    Q_INVOKABLE void enablePlugin(const QString &name, bool on);
    Q_INVOKABLE void updatePlugins();

signals:
    void settingsChanged();
    void statusChanged();
    void defaultChanged();
    void selectedChanged();
    void detailsChanged();
    void pendingChanged();
    void protectionChanged();
    void trayChanged();
    void searchChanged();
    void pluginsChanged();
    /// The add sheet should open (pending has something to show).
    void addRequested();
    /// Something to say for a moment (added, removed, failed...).
    void notice(const QString &text);

private:
    void loadSettings();
    void connectEngine();
    void poll();
    void refreshDetails();
    void setStatus(const QString &status, const QString &text);
    void applyPreferences();
    void updateDefault();
    void done(const Reply &r, const QString &failed);
    // protect.cpp
    void checkProtection();
    void setProtection(const QVariantMap &p);
    /// When the sandboxed engine can't write to `folder` yet: lets it, restarts it and runs
    /// `then` once it is back. Returns false when nothing needed doing (run `then` yourself).
    bool makeWritable(const QString &folder, std::function<void()> then);
    void engineReady();
    // search.cpp
    void pollSearch();
    void pluginCall(const QString &path, const QUrlQuery &form, const QString &okText);
    QTimer m_protectTimer;
    QVariantMap m_protection;
    QString m_bound;            // the engine's current_network_interface, as last seen
    bool m_boundKnown = false;
    bool m_binding = false;
    bool m_blocklistUpdating = false;
    QString m_blocklistError;
    std::function<void()> m_afterReady;
    QTimer m_searchTimer;
    int m_searchId = -1;
    QString m_searchState = QStringLiteral("idle");
    QString m_searchError;
    QVariantList m_results;
    int m_searchTotal = 0;
    QVariantList m_plugins;
    bool m_pluginsBusy = false;
    QStringList m_initial;
    Config m_config;
    QVariantMap m_settings;
    Client *m_client = nullptr;
    TorrentModel m_torrents;
    uqtheme::Watcher m_theme{tokensPath()};
    QFileSystemWatcher m_configWatch;
    QTimer m_poll;
    QTimer m_detailsTimer;
    QPointer<QQuickWindow> m_window;
    QString m_status = QStringLiteral("starting");
    QString m_statusText;
    QString m_engineVersion;
    QString m_selected;
    QVariantMap m_details;
    QVariantList m_pending;
    qint64 m_rid = 0;
    int m_tries = 0;
    bool m_polling = false;
    bool m_startedEngine = false;
    bool m_isDefault = false;
    bool m_trayShown = false;
};

} // namespace uqbit

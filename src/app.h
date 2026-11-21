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

signals:
    void settingsChanged();
    void statusChanged();
    void defaultChanged();
    void selectedChanged();
    void detailsChanged();
    void pendingChanged();
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
};

} // namespace uqbit

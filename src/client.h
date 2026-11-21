// A small client for qBittorrent's Web API (v2) on 127.0.0.1. It logs in by itself (and again
// when the engine restarts) and hands every answer to a callback. The CLI uses `wait()` to run
// one request to the end.
#pragma once

#include "config.h"

#include <QByteArray>
#include <QHttpMultiPart>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrlQuery>
#include <functional>

namespace uqbit {

struct Reply {
    int status = 0;   // HTTP status; 0 when the engine couldn't be reached
    QByteArray body;
    QString error;    // "" on success (2xx)
    bool ok() const { return error.isEmpty(); }
};

/// One file part of an upload (torrents/add).
struct Upload {
    QString name;
    QByteArray data;
};

class Client : public QObject {
    Q_OBJECT
public:
    using Done = std::function<void(const Reply &)>;

    explicit Client(int port, QString password, QObject *parent = nullptr);

    /// GET /api/v2/<path>?<query>.
    void get(const QString &path, const QUrlQuery &query, Done done);
    /// POST /api/v2/<path> as a form, or as multipart when there are uploads.
    void post(const QString &path, const QUrlQuery &form, Done done,
              const QList<Upload> &uploads = {});

    /// Runs a request to the end (with its own event loop) and returns what it said.
    Reply wait(const std::function<void(Done)> &request, int timeoutMs = 15000);

    // ---- the calls uQbit makes
    void version(Done done);
    void maindata(qint64 rid, Done done);
    /// Magnet links / URLs and .torrent files, saved to `folder` ("" = the engine's default).
    void add(const QStringList &urls, const QList<Upload> &files, const QString &folder,
             bool start, Done done);
    /// "start" / "stop" (qBittorrent 5) with a fallback to "resume" / "pause" (4.x).
    void startStop(const QStringList &hashes, bool start, Done done);
    void remove(const QStringList &hashes, bool deleteFiles, Done done);
    void hashesAction(const QString &action, const QStringList &hashes, Done done);
    void setFilePriority(const QString &hash, const QList<int> &ids, int priority, Done done);
    void setPreferences(const QJsonObject &prefs, Done done);
    void shutdown(Done done);
    void preferences(Done done);

    /// Fetches something from the internet (the blocklist): through the system's proxy, not
    /// the engine, at most `limit` bytes. `done` gets the body or an error.
    void download(const QUrl &url, qint64 limit, Done done);

    int port() const { return m_port; }

private:
    void send(const QString &method, const QString &path, const QUrlQuery &query,
              const QList<Upload> &uploads, Done done, bool retried);
    void login(const std::function<void(bool, const QString &)> &then);
    QNetworkAccessManager m_net;
    QNetworkAccessManager *m_web = nullptr;
    int m_port;
    QString m_password;
    bool m_loggedIn = false;
};

/// The engine preferences uQbit's settings stand for (save path, limits, ratio, the
/// "finished" notification through `<self> notify-finished`).
QJsonObject preferencesFor(const Config &config, const QString &self);

/// Binds the engine to `interface` ("" = any) and, when that changed, stops and restarts the
/// running torrents: libtorrent keeps connections it already has when only the binding
/// changes, so without this a peer reached outside the VPN would stay connected.
void bindEngine(Client *client, const QString &interface, std::function<void(const Reply &)> done);

/// Downloads the blocklist (blocklist_url), stores it and has the engine load it again.
/// `done` gets "" or an error message.
void updateBlocklist(Client *client, const Config &config,
                     std::function<void(const QString &)> done);

} // namespace uqbit

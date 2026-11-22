#include "client.h"
#include "trackers.h"

#include "engine.h"
#include "privacy.h"
#include "torrents.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QTimer>

#include <memory>

namespace uqbit {

Client::Client(int port, QString password, QObject *parent)
    : QObject(parent), m_port(port), m_password(std::move(password))
{
    // Never through a proxy from the environment: the engine is on this machine.
    m_net.setProxy(QNetworkProxy::NoProxy);
    m_net.setAutoDeleteReplies(true);
    m_net.setTransferTimeout(20000);
}

void Client::get(const QString &path, const QUrlQuery &query, Done done)
{
    send(QStringLiteral("GET"), path, query, {}, std::move(done), false);
}

void Client::post(const QString &path, const QUrlQuery &form, Done done,
                  const QList<Upload> &uploads)
{
    send(QStringLiteral("POST"), path, form, uploads, std::move(done), false);
}

static QUrl apiUrl(int port, const QString &path)
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1/api/v2/%2").arg(port).arg(path));
}

void Client::login(const std::function<void(bool, const QString &)> &then)
{
    QNetworkRequest req(apiUrl(m_port, QStringLiteral("auth/login")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("username"), QString::fromLatin1("uqbit"));
    form.addQueryItem(QStringLiteral("password"), m_password);
    QNetworkReply *r = m_net.post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(r, &QNetworkReply::finished, this, [this, r, then] {
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = r->readAll();
        if (r->error() == QNetworkReply::ConnectionRefusedError || status == 0) {
            m_loggedIn = false;
            then(false, QStringLiteral("the torrent engine isn't running"));
            return;
        }
        // 200 "Ok." (4.x) or 204 (5.x) logs in; 200 "Fails." doesn't.
        m_loggedIn = (status == 200 && body.startsWith("Ok")) || status == 204;
        then(m_loggedIn, m_loggedIn ? QString()
                                    : (status == 403 ? QStringLiteral("the engine refused uQbit's login (too many tries; it unblocks after a while)")
                                                     : QStringLiteral("the engine refused uQbit's password")));
    });
}

void Client::send(const QString &method, const QString &path, const QUrlQuery &query,
                  const QList<Upload> &uploads, Done done, bool retried)
{
    if (!m_loggedIn) {
        login([=, this](bool ok, const QString &why) {
            if (!ok) {
                done({0, {}, why});
                return;
            }
            send(method, path, query, uploads, done, true);
        });
        return;
    }
    QUrl url = apiUrl(m_port, path);
    QNetworkReply *r = nullptr;
    if (method == "GET") {
        url.setQuery(query);
        r = m_net.get(QNetworkRequest(url));
    } else if (!uploads.isEmpty() || path == QStringLiteral("torrents/add")) {
        // torrents/add only takes multipart/form-data.
        auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        for (const auto &item : query.queryItems(QUrl::FullyDecoded)) {
            QHttpPart part;
            part.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QStringLiteral("form-data; name=\"%1\"").arg(item.first));
            part.setBody(item.second.toUtf8());
            multi->append(part);
        }
        for (const Upload &u : uploads) {
            QHttpPart part;
            QString safe = u.name;
            safe.replace('"', '_').replace('\n', '_').replace('\r', '_');
            part.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QStringLiteral("form-data; name=\"torrents\"; filename=\"%1\"").arg(safe));
            part.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-bittorrent");
            part.setBody(u.data);
            multi->append(part);
        }
        r = m_net.post(QNetworkRequest(url), multi);
        multi->setParent(r);
    } else {
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        r = m_net.post(req, query.toString(QUrl::FullyEncoded).toUtf8());
    }
    connect(r, &QNetworkReply::finished, this, [=, this] {
        Reply out;
        out.status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        out.body = r->readAll();
        if (out.status == 403 && !retried) {
            // The session ended (the engine restarted): log in again, once.
            m_loggedIn = false;
            send(method, path, query, uploads, done, true);
            return;
        }
        if (out.status == 0) {
            m_loggedIn = false;
            out.error = QStringLiteral("the torrent engine isn't running");
        } else if (out.status < 200 || out.status >= 300) {
            const QString said = QString::fromUtf8(out.body.left(200)).trimmed();
            out.error = said.isEmpty() ? QStringLiteral("the engine said %1").arg(out.status)
                                       : said;
        }
        done(out);
    });
}

Reply Client::wait(const std::function<void(Done)> &request, int timeoutMs)
{
    // Shared, so an answer that comes after the timeout (during a later wait) lands in a
    // state that is still alive instead of in this returned-from frame.
    struct State {
        QEventLoop loop;
        Reply result{0, {}, QStringLiteral("the torrent engine didn't answer")};
        bool finished = false;
        bool gaveUp = false;
    };
    auto st = std::make_shared<State>();
    request([st](const Reply &r) {
        if (st->gaveUp)
            return;
        st->result = r;
        st->finished = true;
        st->loop.quit();
    });
    if (!st->finished) {
        QTimer::singleShot(timeoutMs, &st->loop, &QEventLoop::quit);
        st->loop.exec();
    }
    st->gaveUp = true;
    return st->result;
}

void Client::version(Done done) { get(QStringLiteral("app/version"), {}, std::move(done)); }

void Client::maindata(qint64 rid, Done done)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("rid"), QString::number(rid));
    get(QStringLiteral("sync/maindata"), q, std::move(done));
}

void Client::add(const QStringList &urls, const QList<Upload> &files, const QString &folder,
                 bool start, Done done)
{
    QUrlQuery form;
    if (!urls.isEmpty())
        form.addQueryItem(QStringLiteral("urls"), urls.join('\n'));
    if (!folder.isEmpty())
        form.addQueryItem(QStringLiteral("savepath"), folder);
    // "stopped" in qBittorrent 5, "paused" before it.
    form.addQueryItem(QStringLiteral("stopped"), start ? "false" : "true");
    form.addQueryItem(QStringLiteral("paused"), start ? "false" : "true");
    send(QStringLiteral("POST"), QStringLiteral("torrents/add"), form, files, std::move(done),
         false);
}

void Client::startStop(const QStringList &hashes, bool start, Done done)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("hashes"), hashes.join('|'));
    const QString now = start ? "torrents/start" : "torrents/stop";
    const QString old = start ? "torrents/resume" : "torrents/pause";
    post(now, form, [=, this](const Reply &r) {
        if (r.status == 404)
            post(old, form, done);
        else
            done(r);
    });
}

void Client::remove(const QStringList &hashes, bool deleteFiles, Done done)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("hashes"), hashes.join('|'));
    form.addQueryItem(QStringLiteral("deleteFiles"), deleteFiles ? "true" : "false");
    post(QStringLiteral("torrents/delete"), form, std::move(done));
}

void Client::hashesAction(const QString &action, const QStringList &hashes, Done done)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("hashes"), hashes.join('|'));
    post(QStringLiteral("torrents/") + action, form, std::move(done));
}

void Client::setFilePriority(const QString &hash, const QList<int> &ids, int priority, Done done)
{
    QStringList idList;
    for (int i : ids)
        idList.append(QString::number(i));
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("hash"), hash);
    form.addQueryItem(QStringLiteral("id"), idList.join('|'));
    form.addQueryItem(QStringLiteral("priority"), QString::number(priority));
    post(QStringLiteral("torrents/filePrio"), form, std::move(done));
}

void Client::setPreferences(const QJsonObject &prefs, Done done)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("json"),
                      QString::fromUtf8(QJsonDocument(prefs).toJson(QJsonDocument::Compact)));
    post(QStringLiteral("app/setPreferences"), form, std::move(done));
}

void Client::shutdown(Done done) { post(QStringLiteral("app/shutdown"), {}, std::move(done)); }

void Client::preferences(Done done) { get(QStringLiteral("app/preferences"), {}, std::move(done)); }

void Client::download(const QUrl &url, qint64 limit, Done done)
{
    if (!m_web) {
        m_web = new QNetworkAccessManager(this);
        m_web->setAutoDeleteReplies(true);
        m_web->setTransferTimeout(60000);
        m_web->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    }
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("uQbit/" UQBIT_VERSION));
    QNetworkReply *r = m_web->get(req);
    connect(r, &QNetworkReply::downloadProgress, r, [r, limit](qint64 got, qint64) {
        if (got > limit)
            r->abort();
    });
    connect(r, &QNetworkReply::finished, this, [r, limit, done] {
        Reply out;
        out.status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (r->error() == QNetworkReply::OperationCanceledError)
            out.error = QStringLiteral("the download is bigger than %1 MiB").arg(limit >> 20);
        else if (r->error() != QNetworkReply::NoError)
            out.error = r->errorString();
        else
            out.body = r->readAll();
        done(out);
    });
}

void bindEngine(Client *client, const QString &interface, std::function<void(const Reply &)> done)
{
    client->preferences([=](const Reply &r) {
        const QString now = QJsonDocument::fromJson(r.body).object()
                                .value("current_network_interface").toString();
        if (r.ok() && now == interface) {
            done(r);
            return;
        }
        client->setPreferences({{"current_network_interface", interface},
                                {"current_interface_address", QString()}},
                               [=](const Reply &set) {
            if (!set.ok()) {
                done(set);
                return;
            }
            client->get("torrents/info", {}, [=](const Reply &info) {
                QStringList running;
                for (const QJsonValue &v : QJsonDocument::fromJson(info.body).array()) {
                    const QJsonObject t = v.toObject();
                    if (stateGroup(t.value("state").toString()) != "stopped")
                        running << t.value("hash").toString();
                }
                if (running.isEmpty()) {
                    done(set);
                    return;
                }
                client->startStop(running, false, [=](const Reply &) {
                    QTimer::singleShot(1500, client, [=] {
                        client->startStop(running, true, [=](const Reply &) { done(set); });
                    });
                });
            });
        });
    });
}

void updateBlocklist(Client *client, const Config &config,
                     std::function<void(const QString &)> done)
{
    const QUrl url(config.text("blocklist_url"));
    client->download(url, 64 << 20, [client, done](const Reply &r) {
        if (!r.ok()) {
            done(QStringLiteral("couldn't download the blocklist: %1").arg(r.error));
            return;
        }
        QString error;
        const QString path = storeBlocklist(r.body, &error);
        if (path.isEmpty()) {
            done(error);
            return;
        }
        // Off and on again: the engine reads the file only when filtering turns on or the
        // path changes.
        client->setPreferences({{"ip_filter_enabled", false}}, [client, path, done](const Reply &) {
            client->setPreferences({{"ip_filter_enabled", true},
                                    {"ip_filter_path", path},
                                    {"ip_filter_trackers", true}},
                                   [done](const Reply &r) { done(r.ok() ? QString() : r.error); });
        });
    });
}

QJsonObject preferencesFor(const Config &config, const QString &self)
{
    const int ratio = config.number("seed_ratio");
    QString quoted = self;
    quoted.replace('"', "\\\"");
    const QString list = config.boolean("blocklist") ? blocklistPath() : QString();
    const int seedTime = config.number("seed_time");
    const QString layout = config.text("content_layout");
    const QString incomplete = expandHome(config.text("incomplete_folder"));
    const QString watch = expandHome(config.text("watch_folder"));
    // 0 in uQbit's settings is "no limit", which the engine spells -1; no limits at all turns
    // the queue off.
    const auto active = [&](const char *key) {
        const int n = config.number(QString::fromLatin1(key));
        return n > 0 ? n : -1;
    };
    const bool queueing = config.number("max_active_downloads") > 0
                          || config.number("max_active_uploads") > 0
                          || config.number("max_active_torrents") > 0;
    QJsonObject prefs{
        {"save_path", expandHome(config.text("download_folder"))},
        {"dl_limit", config.number("download_limit") * 1024},
        {"up_limit", config.number("upload_limit") * 1024},
        {"max_ratio_enabled", ratio > 0},
        {"max_ratio", ratio > 0 ? double(ratio) : -1.0},
        {"max_seeding_time_enabled", seedTime > 0},
        {"max_seeding_time", seedTime > 0 ? seedTime : -1},
        // 0 stops the torrent, 1 removes it (its files stay).
        {"max_ratio_act", config.text("when_seeded") == "remove" ? 1 : 0},
        {"temp_path_enabled", !incomplete.isEmpty()},
        {"incomplete_files_ext", config.boolean("incomplete_extension")},
        {"preallocate_all", config.boolean("preallocate")},
        {"torrent_content_layout", layout == "subfolder" ? "Subfolder"
                                   : layout == "no_subfolder" ? "NoSubfolder" : "Original"},
        {"queueing_enabled", queueing},
        {"max_active_downloads", active("max_active_downloads")},
        {"max_active_uploads", active("max_active_uploads")},
        {"max_active_torrents", active("max_active_torrents")},
        // Folders the engine watches for .torrent files; 1 = save to the default folder. The
        // list replaces the engine's, so an empty one stops watching.
        {"scan_dirs", watch.isEmpty() ? QJsonObject{} : QJsonObject{{watch, 1}}},
        {"autorun_enabled", config.boolean("notify_finished")},
        {"autorun_program", QStringLiteral("\"%1\" notify-finished \"%N\" \"%F\"").arg(quoted)},
        {"encryption", encryptionMode(config)},
        {"anonymous_mode", config.boolean("anonymous_mode")},
        {"ip_filter_enabled", !list.isEmpty()},
        {"ip_filter_trackers", !list.isEmpty()},
    };
    if (!list.isEmpty())
        prefs.insert("ip_filter_path", list);
    if (!incomplete.isEmpty())
        prefs.insert("temp_path", incomplete);
    // Tracker lists: what uQbit merged from them last (trackers.cpp keeps it fresh).
    const QStringList trackers = trackerListUrls(config).isEmpty() ? QStringList() : storedTrackers();
    prefs.insert("add_trackers_enabled", !trackers.isEmpty());
    prefs.insert("add_trackers", trackers.join('\n'));
    return prefs;
}

} // namespace uqbit

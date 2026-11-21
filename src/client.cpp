#include "client.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QTimer>

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
    QEventLoop loop;
    Reply result{0, {}, QStringLiteral("the torrent engine didn't answer")};
    bool finished = false;
    request([&](const Reply &r) {
        result = r;
        finished = true;
        loop.quit();
    });
    if (!finished) {
        QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return result;
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

QJsonObject preferencesFor(const Config &config, const QString &self)
{
    const int ratio = config.number("seed_ratio");
    QString quoted = self;
    quoted.replace('"', "\\\"");
    return {
        {"save_path", expandHome(config.text("download_folder"))},
        {"dl_limit", config.number("download_limit") * 1024},
        {"up_limit", config.number("upload_limit") * 1024},
        {"max_ratio_enabled", ratio > 0},
        {"max_ratio", ratio > 0 ? double(ratio) : -1.0},
        {"max_ratio_act", 0}, // stop the torrent
        {"autorun_enabled", config.boolean("notify_finished")},
        {"autorun_program", QStringLiteral("\"%1\" notify-finished \"%N\" \"%F\"").arg(quoted)},
    };
}

} // namespace uqbit

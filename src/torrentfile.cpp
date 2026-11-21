#include "torrentfile.h"

#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>

#include <functional>

namespace uqbit {

namespace {

// A bencode value: an integer, a byte string, a list or a dictionary.
struct Reader {
    const QByteArray &d;
    qsizetype i = 0;
    int depth = 0;
    bool bad = false;

    QVariant fail()
    {
        bad = true;
        return {};
    }

    QVariant value()
    {
        if (bad || i >= d.size() || ++depth > 64)
            return fail();
        QVariant out;
        const char c = d.at(i);
        if (c == 'i') {
            const qsizetype end = d.indexOf('e', i);
            if (end < 0)
                return fail();
            bool ok = false;
            out = d.mid(i + 1, end - i - 1).toLongLong(&ok);
            if (!ok)
                return fail();
            i = end + 1;
        } else if (c == 'l') {
            ++i;
            QVariantList list;
            while (!bad && i < d.size() && d.at(i) != 'e')
                list.append(value());
            if (bad || i >= d.size())
                return fail();
            ++i;
            out = list;
        } else if (c == 'd') {
            ++i;
            QVariantMap map;
            while (!bad && i < d.size() && d.at(i) != 'e') {
                const QVariant k = value();
                if (k.typeId() != QMetaType::QByteArray)
                    return fail();
                map.insert(QString::fromUtf8(k.toByteArray()), value());
            }
            if (bad || i >= d.size())
                return fail();
            ++i;
            out = map;
        } else if (c >= '0' && c <= '9') {
            const qsizetype colon = d.indexOf(':', i);
            if (colon < 0)
                return fail();
            bool ok = false;
            const qint64 len = d.mid(i, colon - i).toLongLong(&ok);
            if (!ok || len < 0 || colon + 1 + len > d.size())
                return fail();
            out = d.mid(colon + 1, len);
            i = colon + 1 + len;
        } else {
            return fail();
        }
        --depth;
        return out;
    }
};

QString text(const QVariant &v) { return QString::fromUtf8(v.toByteArray()); }

} // namespace

QJsonObject describeTorrent(const QByteArray &data)
{
    if (data.size() > 64 * 1024 * 1024)
        return {{"ok", false}, {"error", "this file is too big to be a torrent"}};
    Reader r{data};
    const QVariantMap root = r.value().toMap();
    const QVariantMap info = root.value("info").toMap();
    if (r.bad || info.isEmpty())
        return {{"ok", false}, {"error", "this isn't a torrent file"}};
    const QString name = text(info.value("name.utf-8", info.value("name")));
    qint64 size = 0;
    QJsonArray files;
    if (info.contains("files")) {
        for (const QVariant &f : info.value("files").toList()) {
            const QVariantMap m = f.toMap();
            const qint64 len = m.value("length").toLongLong();
            QStringList parts;
            for (const QVariant &p : m.value("path.utf-8", m.value("path")).toList())
                parts.append(text(p));
            size += len;
            if (files.size() < 500)
                files.append(QJsonObject{{"path", parts.join('/')}, {"size", len}});
        }
    } else if (info.contains("file tree")) { // v2-only torrents
        std::function<void(const QVariantMap &, const QString &)> walk =
            [&](const QVariantMap &tree, const QString &prefix) {
                for (auto it = tree.begin(); it != tree.end(); ++it) {
                    const QVariantMap node = it.value().toMap();
                    if (it.key().isEmpty()) {
                        const qint64 len = node.value("length").toLongLong();
                        size += len;
                        if (files.size() < 500)
                            files.append(QJsonObject{{"path", prefix}, {"size", len}});
                    } else {
                        walk(node, prefix.isEmpty() ? it.key() : prefix + '/' + it.key());
                    }
                }
            };
        walk(info.value("file tree").toMap(), QString());
    } else {
        size = info.value("length").toLongLong();
        files.append(QJsonObject{{"path", name}, {"size", size}});
    }
    int count = 0;
    if (info.contains("files"))
        count = int(info.value("files").toList().size());
    else
        count = int(files.size());
    return {{"ok", true}, {"name", name}, {"size", size}, {"files", files}, {"file_count", count}};
}

QJsonObject describeMagnet(const QString &link)
{
    const QUrl url(link.trimmed());
    if (url.scheme() != QStringLiteral("magnet"))
        return {{"ok", false}, {"error", "this isn't a magnet link"}};
    const QUrlQuery q(url.query(QUrl::FullyEncoded));
    QString hash;
    int trackers = 0;
    QString name;
    for (const auto &item : q.queryItems(QUrl::FullyDecoded)) {
        if (item.first == "xt" && hash.isEmpty()) {
            for (const char *prefix : {"urn:btih:", "urn:btmh:"})
                if (item.second.startsWith(QLatin1String(prefix)))
                    hash = item.second.mid(int(qstrlen(prefix))).toLower();
        } else if (item.first == "dn" && name.isEmpty()) {
            name = item.second;
            name.replace('+', ' ');
        } else if (item.first == "tr") {
            ++trackers;
        }
    }
    if (hash.isEmpty())
        return {{"ok", false}, {"error", "this magnet link has no torrent hash"}};
    return {{"ok", true}, {"name", name}, {"hash", hash}, {"trackers", trackers}};
}

bool isTorrentLink(const QString &text)
{
    const QString t = text.trimmed();
    if (t.startsWith(QLatin1String("magnet:?"), Qt::CaseInsensitive))
        return true;
    return (t.startsWith(QLatin1String("http://")) || t.startsWith(QLatin1String("https://")))
           && QUrl(t).path().endsWith(QLatin1String(".torrent"), Qt::CaseInsensitive);
}

} // namespace uqbit

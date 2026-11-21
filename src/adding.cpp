// The "add" side of App: links and files in, the add sheet, and sending them to the engine.
#include "app.h"
#include "torrentfile.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QUrl>

namespace uqbit {

void App::openInputs(const QStringList &inputs)
{
    for (const QString &raw : inputs) {
        const QString in = raw.trimmed();
        if (in.isEmpty())
            continue;
        QVariantMap item{{"input", in}};
        if (in.startsWith(QLatin1String("magnet:"), Qt::CaseInsensitive)) {
            const QJsonObject m = describeMagnet(in);
            item.insert("kind", "magnet");
            item.insert("ok", m.value("ok").toBool());
            item.insert("error", m.value("error").toString());
            item.insert("name", m.value("name").toString());
            item.insert("hash", m.value("hash").toString());
        } else if (in.startsWith(QLatin1String("http://")) || in.startsWith(QLatin1String("https://"))) {
            item.insert("kind", "url");
            item.insert("ok", true);
            item.insert("name", QUrl(in).fileName());
        } else {
            const QString path = in.startsWith(QLatin1String("file://")) ? QUrl(in).toLocalFile()
                                                                         : expandHome(in);
            QFile f(path);
            item.insert("kind", "file");
            item.insert("input", path);
            if (!f.open(QIODevice::ReadOnly) || f.size() > 64 * 1024 * 1024) {
                item.insert("ok", false);
                item.insert("error", tr("Can't read this file"));
                item.insert("name", QFileInfo(path).fileName());
            } else {
                const QJsonObject t = describeTorrent(f.readAll());
                item.insert("ok", t.value("ok").toBool());
                item.insert("error", t.value("error").toString());
                item.insert("name", t.value("name").toString(QFileInfo(path).fileName()));
                item.insert("size", t.value("size").toDouble());
                item.insert("file_count", t.value("file_count").toInt());
                item.insert("files", t.value("files").toArray().toVariantList());
            }
        }
        m_pending.append(item);
    }
    emit pendingChanged();
    if (m_pending.isEmpty())
        return;
    if (!m_config.boolean("ask_before_adding"))
        addPending(QString(), m_config.boolean("start_added"));
    else
        emit addRequested();
}

int App::pasteLinks()
{
    QStringList links;
    for (const QString &line : QGuiApplication::clipboard()->text().split('\n'))
        if (isTorrentLink(line))
            links.append(line.trimmed());
    if (!links.isEmpty())
        openInputs(links);
    return int(links.size());
}

QString App::downloadFolder() const { return expandHome(m_config.text("download_folder")); }

QString App::prettyFolder(const QString &folder) const
{
    QString p = folder.startsWith(QLatin1String("file://")) ? QUrl(folder).toLocalFile() : folder;
    if (p == QDir::homePath() || p.startsWith(QDir::homePath() + '/'))
        p = '~' + p.mid(QDir::homePath().size());
    return p;
}

void App::addPending(const QString &folderIn, bool start)
{
    if (!m_client || m_status != "ready") {
        emit notice(tr("The torrent engine isn't running yet"));
        return;
    }
    QString folder = folderIn.startsWith(QLatin1String("file://")) ? QUrl(folderIn).toLocalFile()
                                                                   : expandHome(folderIn);
    if (folder.isEmpty())
        folder = downloadFolder();
    QStringList urls;
    QList<Upload> files;
    for (const QVariant &v : std::as_const(m_pending)) {
        const QVariantMap item = v.toMap();
        if (!item.value("ok").toBool())
            continue;
        if (item.value("kind") == "file") {
            QFile f(item.value("input").toString());
            if (f.open(QIODevice::ReadOnly))
                files.append({QFileInfo(f.fileName()).fileName(), f.readAll()});
        } else {
            urls.append(item.value("input").toString());
        }
    }
    const int n = int(urls.size() + files.size());
    m_pending.clear();
    emit pendingChanged();
    if (n == 0)
        return;
    m_client->add(urls, files, folder, start, [this, n, folder](const Reply &r) {
        if (!r.ok())
            emit notice(tr("Couldn't add: %1").arg(r.error));
        else if (r.body.trimmed().startsWith("Fails"))
            emit notice(tr("Already added, or not a torrent"));
        else
            emit notice(n == 1 ? tr("Added to %1").arg(prettyFolder(folder))
                               : tr("Added %1 torrents to %2").arg(n).arg(prettyFolder(folder)));
        poll();
    });
}

void App::dropPending(int index)
{
    if (index < 0 || index >= m_pending.size())
        return;
    m_pending.removeAt(index);
    emit pendingChanged();
}

void App::clearPending()
{
    m_pending.clear();
    emit pendingChanged();
}

} // namespace uqbit

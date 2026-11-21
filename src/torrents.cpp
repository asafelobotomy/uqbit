#include "torrents.h"

#include <QJsonArray>
#include <algorithm>

namespace uqbit {

QString stateGroup(const QString &state)
{
    if (state == "error" || state == "missingFiles" || state == "unknown")
        return QStringLiteral("error");
    if (state.startsWith("stopped") || state.startsWith("paused"))
        return QStringLiteral("stopped");
    if (state.startsWith("checking") || state == "moving" || state == "allocating")
        return QStringLiteral("checking");
    if (state.endsWith("UP") || state == "uploading")
        return QStringLiteral("seeding");
    return QStringLiteral("downloading");
}

TorrentModel::TorrentModel(QObject *parent) : QAbstractListModel(parent) {}

int TorrentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> TorrentModel::roleNames() const
{
    return {
        {HashRole, "hash"}, {NameRole, "name"}, {StateRole, "state"}, {GroupRole, "group"},
        {ProgressRole, "progress"}, {SizeRole, "size"}, {DownloadedRole, "downloaded"},
        {UploadedRole, "uploaded"}, {DlSpeedRole, "dlspeed"}, {UpSpeedRole, "upspeed"},
        {EtaRole, "eta"}, {RatioRole, "ratio"}, {SeedsRole, "seeds"}, {PeersRole, "peers"},
        {AddedRole, "addedOn"}, {SavePathRole, "savePath"}, {ContentPathRole, "contentPath"},
        {MagnetRole, "magnet"}, {CategoryRole, "category"}, {CompletedOnRole, "completedOn"},
    };
}

QVariant TorrentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const QString hash = m_rows.at(index.row());
    const QVariantMap t = m_all.value(hash);
    switch (role) {
    case HashRole: return hash;
    case NameRole: return t.value("name");
    case StateRole: return t.value("state");
    case GroupRole: return stateGroup(t.value("state").toString());
    case ProgressRole: return t.value("progress").toDouble();
    case SizeRole: return t.value("size").toLongLong();
    case DownloadedRole: return t.value("downloaded").toLongLong();
    case UploadedRole: return t.value("uploaded").toLongLong();
    case DlSpeedRole: return t.value("dlspeed").toLongLong();
    case UpSpeedRole: return t.value("upspeed").toLongLong();
    case EtaRole: return t.value("eta").toLongLong();
    case RatioRole: return t.value("ratio").toDouble();
    case SeedsRole: return t.value("num_seeds").toInt();
    case PeersRole: return t.value("num_leechs").toInt();
    case AddedRole: return t.value("added_on").toLongLong();
    case SavePathRole: return t.value("save_path");
    case ContentPathRole: return t.value("content_path");
    case MagnetRole: return t.value("magnet_uri");
    case CategoryRole: return t.value("category");
    case CompletedOnRole: return t.value("completion_on").toLongLong();
    }
    return {};
}

bool TorrentModel::shown(const QVariantMap &t) const
{
    if (!m_search.isEmpty() && !t.value("name").toString().contains(m_search, Qt::CaseInsensitive))
        return false;
    const QString g = stateGroup(t.value("state").toString());
    if (m_filter == "all")
        return true;
    if (m_filter == "finished")
        return t.value("progress").toDouble() >= 1.0;
    if (m_filter == "downloading")
        return g == "downloading" || (g == "checking" && t.value("progress").toDouble() < 1.0);
    return g == m_filter;
}

void TorrentModel::relayout()
{
    QStringList next;
    for (auto it = m_all.cbegin(); it != m_all.cend(); ++it)
        if (shown(it.value()))
            next.append(it.key());
    std::sort(next.begin(), next.end(), [this](const QString &a, const QString &b) {
        const qint64 ta = m_all.value(a).value("added_on").toLongLong();
        const qint64 tb = m_all.value(b).value("added_on").toLongLong();
        return ta != tb ? ta > tb : a < b;
    });
    if (next == m_rows) {
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(int(m_rows.size()) - 1));
        return;
    }
    const int before = int(m_rows.size());
    beginResetModel();
    m_rows = next;
    endResetModel();
    if (before != m_rows.size())
        emit countChanged();
}

qint64 TorrentModel::apply(const QJsonObject &md)
{
    const bool full = md.value("full_update").toBool();
    QHash<QString, bool> wasDone;
    for (auto it = m_all.cbegin(); it != m_all.cend(); ++it)
        wasDone.insert(it.key(), it.value().value("progress").toDouble() >= 1.0);
    if (full)
        m_all.clear();
    const QJsonObject torrents = md.value("torrents").toObject();
    for (auto it = torrents.begin(); it != torrents.end(); ++it) {
        QVariantMap &t = m_all[it.key()];
        const QVariantMap changes = it.value().toObject().toVariantMap();
        for (auto c = changes.cbegin(); c != changes.cend(); ++c)
            t.insert(c.key(), c.value());
    }
    for (const QJsonValue &gone : md.value("torrents_removed").toArray())
        m_all.remove(gone.toString());
    const QJsonObject server = md.value("server_state").toObject();
    if (!server.isEmpty() || full) {
        if (full)
            m_server.clear();
        for (auto it = server.begin(); it != server.end(); ++it)
            m_server.insert(it.key(), it.value().toVariant());
        emit serverChanged();
    }
    if (m_haveFull) {
        for (auto it = torrents.begin(); it != torrents.end(); ++it) {
            const QVariantMap &t = m_all.value(it.key());
            if (wasDone.contains(it.key()) && !wasDone.value(it.key())
                && t.value("progress").toDouble() >= 1.0)
                emit finished(it.key(), t.value("name").toString());
        }
    }
    m_haveFull = m_haveFull || full;
    relayout();
    emit countsChanged();
    return md.value("rid").toInteger();
}

void TorrentModel::clear()
{
    m_all.clear();
    m_server.clear();
    m_haveFull = false;
    relayout();
    emit countsChanged();
    emit serverChanged();
}

void TorrentModel::setFilter(const QString &f)
{
    static const QStringList ok = {"all", "downloading", "seeding", "finished", "stopped", "error"};
    const QString v = ok.contains(f) ? f : QStringLiteral("all");
    if (v == m_filter)
        return;
    m_filter = v;
    relayout();
    emit filterChanged();
}

void TorrentModel::setSearch(const QString &s)
{
    if (s == m_search)
        return;
    m_search = s;
    relayout();
    emit searchChanged();
}

QVariantMap TorrentModel::get(const QString &hash) const
{
    QVariantMap t = m_all.value(hash);
    if (!t.isEmpty())
        t.insert("group", stateGroup(t.value("state").toString()));
    return t;
}

QVariantMap TorrentModel::counts() const
{
    int dl = 0, seed = 0, done = 0, stopped = 0, err = 0;
    for (const QVariantMap &t : m_all) {
        const QString g = stateGroup(t.value("state").toString());
        const bool complete = t.value("progress").toDouble() >= 1.0;
        if (g == "downloading" || (g == "checking" && !complete))
            ++dl;
        else if (g == "seeding")
            ++seed;
        else if (g == "stopped")
            ++stopped;
        else if (g == "error")
            ++err;
        if (complete)
            ++done;
    }
    return {{"all", int(m_all.size())}, {"downloading", dl}, {"seeding", seed},
            {"finished", done}, {"stopped", stopped}, {"error", err}};
}

} // namespace uqbit

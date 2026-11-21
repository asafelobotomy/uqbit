// The engine's torrents as the window lists them. `apply()` merges each answer of
// sync/maindata (a full list, or only what changed since the last one) into one table; the
// rows QML sees are that table filtered (all, downloading, seeding, ...) and searched, newest
// first. Pure Qt Core, so tests/core drives it with canned answers.
#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>

namespace uqbit {

/// "downloading", "seeding", "stopped", "error" or "checking" for a qBittorrent state.
QString stateGroup(const QString &state);

class TorrentModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int total READ total NOTIFY countsChanged)
    /// {all, downloading, seeding, finished, stopped, error}: how many of each.
    Q_PROPERTY(QVariantMap counts READ counts NOTIFY countsChanged)
    /// server_state: dl_info_speed, up_info_speed, free_space_on_disk, connection_status, ...
    Q_PROPERTY(QVariantMap server READ server NOTIFY serverChanged)

public:
    enum Role {
        HashRole = Qt::UserRole + 1, NameRole, StateRole, GroupRole, ProgressRole, SizeRole,
        DownloadedRole, UploadedRole, DlSpeedRole, UpSpeedRole, EtaRole, RatioRole, SeedsRole,
        PeersRole, AddedRole, SavePathRole, ContentPathRole, MagnetRole, CategoryRole,
        CompletedOnRole,
    };

    explicit TorrentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Merges one sync/maindata answer. Returns the `rid` to ask with next time.
    qint64 apply(const QJsonObject &maindata);
    /// Forgets everything (the engine went away).
    void clear();

    QString filter() const { return m_filter; }
    void setFilter(const QString &f);
    QString search() const { return m_search; }
    void setSearch(const QString &s);
    int total() const { return int(m_all.size()); }
    QVariantMap counts() const;
    QVariantMap server() const { return m_server; }

    /// One torrent's fields as the engine names them, plus `group` (empty map when it's gone).
    Q_INVOKABLE QVariantMap get(const QString &hash) const;
    Q_INVOKABLE int indexOf(const QString &hash) const { return int(m_rows.indexOf(hash)); }
    /// The hashes shown now, in order.
    QStringList rows() const { return m_rows; }

signals:
    void filterChanged();
    void searchChanged();
    void countChanged();
    void countsChanged();
    void serverChanged();
    /// A torrent that was downloading is now complete (not on the first full list).
    void finished(const QString &hash, const QString &name);

private:
    bool shown(const QVariantMap &t) const;
    void relayout();
    QHash<QString, QVariantMap> m_all;
    QStringList m_rows;
    QVariantMap m_server;
    QString m_filter = QStringLiteral("all");
    QString m_search;
    bool m_haveFull = false;
};

} // namespace uqbit

#include "trackers.h"

#include "client.h"
#include "config.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>

#include <memory>

namespace uqbit {

namespace {
constexpr int kMaxTrackers = 500;
constexpr qsizetype kMaxList = 2 << 20;
} // namespace

QStringList trackerListUrls(const Config &config)
{
    QStringList out;
    for (const QString &u : config.text("tracker_lists").split(' ', Qt::SkipEmptyParts))
        if (u.startsWith(QLatin1String("https://")) && !out.contains(u))
            out << u;
    return out;
}

QStringList parseTrackers(const QByteArray &list)
{
    static const QRegularExpression ok(QStringLiteral("^(udp|https?|wss)://[^\\s\\x00-\\x1f]{1,400}$"));
    QStringList out;
    QSet<QString> seen;
    for (const QByteArray &raw : list.split('\n')) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty() || line.startsWith('#') || !ok.match(line).hasMatch())
            continue;
        const QUrl url(line);
        if (!url.isValid() || url.host().isEmpty() || seen.contains(line))
            continue;
        seen.insert(line);
        out << line;
    }
    return out;
}

QString trackersPath() { return stateDir() + QStringLiteral("/trackers.txt"); }

QStringList storedTrackers()
{
    QFile f(trackersPath());
    if (!f.open(QIODevice::ReadOnly) || f.size() > kMaxList)
        return {};
    return parseTrackers(f.readAll());
}

bool storeTrackers(const QStringList &trackers)
{
    QDir().mkpath(stateDir());
    QSaveFile out(trackersPath());
    return out.open(QIODevice::WriteOnly) && out.write(trackers.join('\n').toUtf8() + '\n') >= 0
           && out.commit();
}

bool trackersStale()
{
    const QFileInfo f(trackersPath());
    return !f.exists() || f.lastModified().secsTo(QDateTime::currentDateTime()) >= 24 * 3600;
}

void updateTrackerLists(Client *client, const Config &config, bool apply,
                        std::function<void(const QString &, int)> done)
{
    const QStringList urls = trackerListUrls(config);
    if (urls.isEmpty()) {
        // No lists: stop adding the old ones.
        QFile::remove(trackersPath());
        if (!apply) {
            done(QString(), 0);
            return;
        }
        client->setPreferences({{"add_trackers_enabled", false}, {"add_trackers", QString()}},
                               [done](const Reply &r) { done(r.ok() ? QString() : r.error, 0); });
        return;
    }
    struct State {
        QStringList merged;
        QSet<QString> seen;
        QStringList errors;
        qsizetype left = 0;
    };
    auto st = std::make_shared<State>();
    st->left = urls.size();
    for (const QString &u : urls)
        client->download(QUrl(u), kMaxList, [client, st, u, apply, done](const Reply &r) {
            if (r.ok()) {
                const QStringList got = parseTrackers(r.body);
                if (got.isEmpty())
                    st->errors << QStringLiteral("%1 has no trackers in it").arg(u);
                for (const QString &t : got)
                    if (st->merged.size() < kMaxTrackers && !st->seen.contains(t)) {
                        st->seen.insert(t);
                        st->merged << t;
                    }
            } else {
                st->errors << QStringLiteral("couldn't download %1: %2").arg(u, r.error);
            }
            if (--st->left > 0)
                return;
            const QString error = st->errors.join(QStringLiteral("; "));
            // Nothing came: keep the list from last time rather than dropping every tracker.
            if (st->merged.isEmpty()) {
                done(error.isEmpty() ? QStringLiteral("the lists had no trackers") : error, 0);
                return;
            }
            if (!storeTrackers(st->merged)) {
                done(QStringLiteral("can't save %1").arg(trackersPath()), 0);
                return;
            }
            const int count = int(st->merged.size());
            if (!apply) {
                done(error, count);
                return;
            }
            client->setPreferences({{"add_trackers_enabled", true},
                                    {"add_trackers", st->merged.join('\n')},
                                    {"add_trackers_from_url_enabled", false}},
                                   [done, error, count](const Reply &r) {
                                       done(r.ok() ? error : r.error, count);
                                   });
        });
}

} // namespace uqbit

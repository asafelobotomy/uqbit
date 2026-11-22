// `uqbit trackers status|update`: the tracker lists added to new public torrents.
#include "cli_util.h"
#include "client.h"
#include "config.h"
#include "engine.h"
#include "trackers.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

namespace uqbit {

QJsonObject trackersJson(const Config &cfg)
{
    const QStringList urls = trackerListUrls(cfg);
    const QFileInfo f(trackersPath());
    const int count = urls.isEmpty() ? 0 : int(storedTrackers().size());
    return {{"lists", QJsonArray::fromStringList(urls)},
            {"count", count},
            {"updated", urls.isEmpty() || !f.exists()
                            ? QJsonValue(QJsonValue::Null)
                            : QJsonValue(f.lastModified().toUTC().toString(Qt::ISODate))}};
}

/// Downloads the lists now; "" or what went wrong. Used by `trackers update` and by
/// `config set tracker_lists`.
QString refreshTrackers(const Config &cfg)
{
    Client c(cfg.number("webui_port"), webPassword());
    const bool running = !engineVersion(c, 1500).isEmpty();
    QString error;
    c.wait([&](Client::Done d) {
        updateTrackerLists(&c, cfg, running, [d, &error](const QString &e, int) {
            error = e;
            d({200, {}, {}});
        });
    }, 90000);
    return error;
}

int cmdTrackers(const QStringList &args, bool asJson, QTextStream &out, QTextStream &err)
{
    Config cfg;
    const QString sub = args.value(1);
    if ((sub == "status" || sub == "update") && args.size() == 2) {
        QString warning;
        if (sub == "update") {
            if (trackerListUrls(cfg).isEmpty())
                return fail(err, "no tracker lists (uqbit config set tracker_lists \"https://…\")");
            warning = refreshTrackers(cfg);
            if (storedTrackers().isEmpty())
                return fail(err, warning.isEmpty() ? QStringLiteral("no trackers came") : warning);
        }
        QJsonObject t = trackersJson(cfg);
        if (!warning.isEmpty())
            t.insert("warning", warning);
        if (asJson) {
            json(out, t);
        } else if (t.value("lists").toArray().isEmpty()) {
            out << "no tracker lists\n";
        } else {
            out << t.value("count").toInt() << " trackers from " << t.value("lists").toArray().size()
                << " list(s)";
            if (!t.value("updated").isNull())
                out << ", updated " << t.value("updated").toString();
            out << '\n';
            if (!warning.isEmpty())
                err << "uqbit: " << warning << '\n';
        }
        return 0;
    }
    return fail(err, "usage: uqbit trackers status|update", 2);
}

} // namespace uqbit

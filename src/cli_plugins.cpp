// `uqbit plugins status|import`: search plugins brought in from qBittorrent's plugin folder.
#include "cli_util.h"
#include "client.h"
#include "config.h"
#include "engine.h"
#include "plugins.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

namespace uqbit {

static QJsonObject pluginsJson(const Config &cfg)
{
    QJsonArray dirs;
    for (const QString &d : qbtPluginDirs())
        if (QFileInfo(d).isDir())
            dirs << d;
    QJsonArray found;
    for (const QbtPlugin &p : qbtPlugins())
        found << QJsonObject{{"name", p.name}, {"path", p.path}};
    return {{"enabled", cfg.boolean("import_plugins")},
            {"folders", dirs},
            {"found", found},
            {"imported", QJsonArray::fromStringList(importedPlugins().keys())}};
}

int cmdPlugins(const QStringList &args, bool asJson, QTextStream &out, QTextStream &err)
{
    Config cfg;
    const QString sub = args.value(1);
    if ((sub != "status" && sub != "import") || args.size() != 2)
        return fail(err, "usage: uqbit plugins status|import", 2);
    QJsonObject o;
    if (sub == "import") {
        Client c(cfg.number("webui_port"), webPassword());
        const QString e = ensureEngine(c);
        if (!e.isEmpty())
            return fail(err, e);
        ImportResult res;
        c.wait([&](Client::Done d) {
            importPlugins(&c, [d, &res](const ImportResult &r) {
                res = r;
                d({200, {}, {}});
            });
        }, 30000);
        o = pluginsJson(cfg);
        o.insert("added", QJsonArray::fromStringList(res.added));
        o.insert("removed", QJsonArray::fromStringList(res.removed));
        if (!res.error.isEmpty())
            o.insert("error", res.error);
    } else {
        o = pluginsJson(cfg);
    }
    if (asJson) {
        json(out, o);
    } else {
        if (o.value("folders").toArray().isEmpty())
            out << "no qBittorrent plugin folder\n";
        for (const QJsonValue &d : o.value("folders").toArray())
            out << "folder: " << d.toString() << '\n';
        out << o.value("found").toArray().size() << " plugin(s) there, "
            << o.value("imported").toArray().size() << " brought into uQbit";
        if (sub == "import")
            out << " (" << o.value("added").toArray().size() << " added or updated, "
                << o.value("removed").toArray().size() << " removed)";
        out << '\n';
    }
    if (o.contains("error"))
        return fail(err, o.value("error").toString());
    return 0;
}

} // namespace uqbit

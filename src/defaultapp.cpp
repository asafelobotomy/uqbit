#include "defaultapp.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSaveFile>
#include <QSet>

namespace uqbit {

QStringList mimeTypes()
{
    return {QStringLiteral("application/x-bittorrent"), QStringLiteral("x-scheme-handler/magnet")};
}

QString userMimeappsPath()
{
    QString cfg = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (!QDir::isAbsolutePath(cfg))
        cfg = QDir::homePath() + "/.config";
    return cfg + "/mimeapps.list";
}

QStringList systemMimeappsPaths()
{
    QStringList out;
    const QString dirs = qEnvironmentVariable("XDG_CONFIG_DIRS", "/etc/xdg");
    for (const QString &d : dirs.split(':', Qt::SkipEmptyParts))
        if (QDir::isAbsolutePath(d))
            out.append(d + "/mimeapps.list");
    out.append("/usr/share/applications/mimeapps.list");
    return out;
}

static QString readSmall(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 1024 * 1024)
        return {};
    return QString::fromUtf8(f.readAll());
}

/// The first desktop id `[Default Applications]` gives `mime` in this text.
static QString lookupIn(const QString &text, const QString &mime)
{
    bool inDefaults = false;
    for (const QString &raw : text.split('\n')) {
        const QString line = raw.trimmed();
        if (line.startsWith('[')) {
            inDefaults = line == QStringLiteral("[Default Applications]");
            continue;
        }
        if (!inDefaults)
            continue;
        const int eq = line.indexOf('=');
        if (eq <= 0 || line.left(eq).trimmed() != mime)
            continue;
        const QStringList ids = line.mid(eq + 1).split(';', Qt::SkipEmptyParts);
        return ids.isEmpty() ? QString() : ids.first().trimmed();
    }
    return {};
}

QString defaultFor(const QString &mime, const QString &user, const QStringList &system)
{
    QStringList files{user};
    files.append(system);
    for (const QString &f : files) {
        const QString id = lookupIn(readSmall(f), mime);
        if (!id.isEmpty())
            return id;
    }
    return {};
}

QJsonObject defaultStatus(const QString &user, const QStringList &system)
{
    int ours = 0;
    QJsonObject others;
    for (const QString &mime : mimeTypes()) {
        const QString id = defaultFor(mime, user, system);
        if (id == kDesktopId)
            ++ours;
        else if (!id.isEmpty())
            others.insert(id, others.value(id).toInt() + 1);
    }
    const int total = int(mimeTypes().size());
    return {
        {"desktop_id", kDesktopId},
        {"types", total},
        {"ours", ours},
        {"default", ours == total},
        {"torrent", defaultFor("application/x-bittorrent", user, system)},
        {"magnet", defaultFor("x-scheme-handler/magnet", user, system)},
        {"others", others},
    };
}

QString withDefaults(const QString &text, const QStringList &types, const QString &id)
{
    QStringList lines = text.split('\n');
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    QSet<QString> pending(types.begin(), types.end());
    int section = -1; // the line of the [Default Applications] header
    int sectionEnd = -1;
    bool inDefaults = false;
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.startsWith('[')) {
            if (inDefaults)
                sectionEnd = i;
            inDefaults = line == QStringLiteral("[Default Applications]");
            if (inDefaults && section < 0)
                section = i;
            continue;
        }
        if (!inDefaults)
            continue;
        const int eq = line.indexOf('=');
        const QString key = eq > 0 ? line.left(eq).trimmed() : QString();
        if (types.contains(key)) {
            lines[i] = key + '=' + id + ';';
            pending.remove(key);
        }
    }
    if (inDefaults && sectionEnd < section)
        sectionEnd = int(lines.size());
    QStringList add;
    for (const QString &t : types)
        if (pending.contains(t))
            add.append(t + '=' + id + ';');
    if (section < 0) {
        if (!lines.isEmpty())
            lines.append(QString());
        lines.append(QStringLiteral("[Default Applications]"));
        lines.append(add);
    } else {
        // Before the blank lines that end the section, if any.
        int at = sectionEnd;
        while (at - 1 > section && lines.at(at - 1).trimmed().isEmpty())
            --at;
        for (int k = 0; k < add.size(); ++k)
            lines.insert(at + k, add.at(k));
    }
    return lines.join('\n') + '\n';
}

QString makeDefault(const QString &user)
{
    const QString next = withDefaults(readSmall(user), mimeTypes(), kDesktopId);
    QDir().mkpath(QFileInfo(user).absolutePath());
    QSaveFile f(user);
    if (!f.open(QIODevice::WriteOnly))
        return QStringLiteral("can't write %1: %2").arg(user, f.errorString());
    f.write(next.toUtf8());
    if (!f.commit())
        return QStringLiteral("can't write %1: %2").arg(user, f.errorString());
    return {};
}

} // namespace uqbit

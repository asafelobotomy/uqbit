#include "autostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace uqbit {

namespace {
constexpr auto kMarker = "X-uQbit-Autostart=true";
}

QString autostartPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/autostart/io.github.asafelobotomy.uqbit.desktop");
}

QString autostartEntry(bool hidden)
{
    return QStringLiteral("[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=uQbit\n"
                          "Comment=Torrents, in the background\n"
                          "Icon=io.github.asafelobotomy.uqbit\n"
                          "TryExec=uqbit\n"
                          "Exec=uqbit%1\n"
                          "Terminal=false\n"
                          "X-GNOME-Autostart-enabled=true\n"
                          "%2\n")
        .arg(hidden ? QStringLiteral(" --hidden") : QString(), QString::fromLatin1(kMarker));
}

bool launchesOnLogin()
{
    QFile f(autostartPath());
    return f.open(QIODevice::ReadOnly) && f.size() < 64 * 1024
           && f.readAll().contains(kMarker);
}

QString setLaunchOnLogin(bool on, bool hidden)
{
    const QString path = autostartPath();
    if (!on) {
        // Only uQbit's own entry: a file someone else put there is theirs.
        if (QFileInfo::exists(path) && launchesOnLogin() && !QFile::remove(path))
            return QStringLiteral("can't remove %1").arg(path);
        return {};
    }
    if (QFileInfo::exists(path) && !launchesOnLogin())
        return QStringLiteral("%1 is another program's; uQbit leaves it alone").arg(path);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(autostartEntry(hidden).toUtf8()) < 0
        || !out.commit())
        return QStringLiteral("can't write %1").arg(path);
    return {};
}

} // namespace uqbit

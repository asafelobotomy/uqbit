// Search plugins from qBittorrent's own plugin folder. qBittorrent, and tools such as
// qbit-plugin-dl, put plugins in <data>/qBittorrent/nova3/engines; uQbit's engine has a profile
// of its own, so it would never see them. uQbit copies them over (through the engine's
// search/installPlugin), updates them when the files change and removes the ones it brought in
// when they go away. Plugins installed in uQbit itself are never touched.
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace uqbit {

class Client;

/// qBittorrent's plugin folders, in the order they are used (a name found in an earlier one
/// wins): native, Flatpak, and the old "data/qBittorrent" path.
QStringList qbtPluginDirs();
/// The engine's own plugin folder (inside uQbit's profile).
QString enginePluginDir();
/// A plugin file name qBittorrent can load (`<identifier>.py`, not one of its own modules).
bool validPluginFile(const QString &fileName);

struct QbtPlugin {
    QString name;  // "piratebay"
    QString path;  // where it is
    QString stamp; // size and modification time, to notice changes without reading it
};
/// The plugins in `dirs` (default: qbtPluginDirs()), one per name.
QList<QbtPlugin> qbtPlugins(const QStringList &dirs = qbtPluginDirs());

/// What uQbit brought in: {name: {sha256, stamp, from}} (<state>/imported-plugins.json).
QJsonObject importedPlugins();
QString importedPluginsPath();

struct ImportResult {
    QStringList added;   // new or changed
    QStringList removed; // gone from qBittorrent's folder
    QString error;
};
/// Brings the engine up to date with qBittorrent's plugin folders. Does nothing (and calls
/// `done` at once) when nothing changed since last time.
void importPlugins(Client *client, std::function<void(const ImportResult &)> done,
                   const QStringList &dirs = qbtPluginDirs());

} // namespace uqbit

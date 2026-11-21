// Whether uQbit opens torrent files and magnet links (mimeapps.list), and making it so.
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace uqbit {

inline constexpr auto kDesktopId = "io.github.asafelobotomy.uqbit.desktop";

/// $XDG_CONFIG_HOME/mimeapps.list (or ~/.config/mimeapps.list).
QString userMimeappsPath();
/// The files after the user's: $XDG_CONFIG_DIRS/mimeapps.list, then the system's.
QStringList systemMimeappsPaths();

/// The default app for one type: the first `[Default Applications]` entry naming it, user
/// file first. Empty when none is set.
QString defaultFor(const QString &mime, const QString &user, const QStringList &system);

/// `{desktop_id, types, ours, default (all ours), torrent, magnet, others: {id: count}}`.
QJsonObject defaultStatus(const QString &user = userMimeappsPath(),
                          const QStringList &system = systemMimeappsPaths());

/// The types uQbit opens: .torrent files and magnet links.
QStringList mimeTypes();

/// Makes uQbit the default for every type in mimeTypes(), in the user's file only, keeping
/// every other line. Returns an error message or an empty string.
QString makeDefault(const QString &user = userMimeappsPath());

/// The text of a mimeapps.list with `types` set to `id` under [Default Applications].
QString withDefaults(const QString &text, const QStringList &types, const QString &id);

} // namespace uqbit

// What a .torrent file or a magnet link says about itself, for the "add" sheet before the
// engine has it: name, size, files, info hash. Small bencode reader, no engine needed.
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace uqbit {

/// `{ok, name, size, files: [{path, size}], file_count, error?}` for a .torrent file's bytes.
QJsonObject describeTorrent(const QByteArray &data);
/// `{ok, name, hash, trackers, error?}` for a magnet: link (name from dn=, hash from xt=).
QJsonObject describeMagnet(const QString &link);
/// Whether a command-line argument or a dropped thing is a magnet link or a torrent URL.
bool isTorrentLink(const QString &text);

} // namespace uqbit

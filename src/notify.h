// "Download finished" desktop notifications (org.freedesktop.Notifications). The engine runs
// `uqbit notify-finished NAME PATH` when a torrent completes, so they come even with the
// window closed.
#pragma once

#include <QString>

namespace uqbit {

void notifyFinished(const QString &name, const QString &path);

} // namespace uqbit

// "Open uQbit when you log in": an XDG autostart entry in ~/.config/autostart, which desktops
// and uQ's Startup page read.
#pragma once

#include <QString>

namespace uqbit {

QString autostartPath();
/// The entry's text: `uqbit --hidden` when it starts in the tray.
QString autostartEntry(bool hidden);
/// Writes or removes the entry; an error, or "" when done.
QString setLaunchOnLogin(bool on, bool hidden);
/// Whether uQbit's own entry is there (another program's file of the same name is not).
bool launchesOnLogin();

} // namespace uqbit

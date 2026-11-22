// Tracker lists: public lists of trackers (ngosang/trackerslist and the like) that uQbit adds to
// new public torrents, so they find more peers. Any number of list URLs (`tracker_lists`,
// space-separated); uQbit downloads them itself, merges them and hands the engine one list
// (`add_trackers`), so it works with every engine version. The engine never adds them to
// private torrents.
#pragma once

#include <QString>
#include <QStringList>

#include <functional>

namespace uqbit {

class Client;
class Config;

/// The list URLs in the setting, in order, without repeats.
QStringList trackerListUrls(const Config &config);
/// The tracker URLs in a downloaded list (one per line, blank lines between tiers; "#"
/// comments), keeping only udp://, http://, https:// and wss:// announce URLs.
QStringList parseTrackers(const QByteArray &list);
/// The merged list uQbit keeps (<state>/trackers.txt); empty when there is none yet.
QStringList storedTrackers();
QString trackersPath();
/// Saves the merged list; false when it can't.
bool storeTrackers(const QStringList &trackers);
/// No list yet, or it's a day old.
bool trackersStale();
/// Downloads every list, merges and stores them and, with `apply`, has the running engine use
/// them (else it gets them when it starts). `done` gets "" or what went wrong (a list that
/// fails is skipped while another one works) and how many trackers there are now.
void updateTrackerLists(Client *client, const Config &config, bool apply,
                        std::function<void(const QString &error, int count)> done);

} // namespace uqbit

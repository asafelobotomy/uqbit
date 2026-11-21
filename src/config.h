// uQbit's settings: one JSON file (~/.config/uqbit/config.json) that the window, the CLI
// (`uqbit config ...`) and uQ Settings' page all read and write. Only known keys with valid
// values are ever written; a bad or missing file means the defaults.
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace uqbit {

/// The blocklist uQbit uses unless told otherwise: Naunter's BT_BlockLists (p2p, gzipped),
/// rebuilt daily from the iBlocklist level 1 and similar lists.
inline constexpr auto kDefaultBlocklist =
    "https://raw.githubusercontent.com/Naunter/BT_BlockLists/master/bt_blocklists.gz";

struct Key {
    QString name;
    QString type;        // "bool", "choice", "int" or "text"
    QVariant fallback;   // the default
    QStringList choices; // for "choice"
    QString pattern;     // for "text": the whole value must match
    int min = 0;         // for "int"
    int max = 0;
    QString help;
};

/// Every setting, in the order the page and `config show` list them.
const QList<Key> &keys();
const Key *findKey(const QString &name);

/// $UQBIT_CONFIG, else $XDG_CONFIG_HOME/uqbit/config.json, else ~/.config/uqbit/config.json.
QString configPath();
/// $XDG_STATE_HOME/uqbit (the engine's password), else ~/.local/state/uqbit.
QString stateDir();
/// $UQBIT_PROFILE, else $XDG_DATA_HOME/uqbit, else ~/.local/share/uqbit: the engine's own
/// profile (qBittorrent keeps its settings, torrents and resume data there).
QString profileDir();
/// "~/x" -> "$HOME/x"; anything else unchanged.
QString expandHome(const QString &path);

class Config {
public:
    explicit Config(QString path = configPath());

    /// Re-reads the file. Unknown keys and bad values are dropped (their defaults apply).
    void load();
    QVariant value(const QString &key) const;
    bool boolean(const QString &key) const { return value(key).toBool(); }
    int number(const QString &key) const { return value(key).toInt(); }
    QString text(const QString &key) const { return value(key).toString(); }

    /// Checks and stores one value (as typed on the command line: "true"/"false" for a bool).
    /// Returns an error message, or an empty string when it was saved.
    QString set(const QString &key, const QString &raw);
    /// Takes the key out of the file, so its default applies again.
    QString reset(const QString &key);

    /// `{settings: [{key, type, default, value, set, choices?, min?, max?, help}], path}`.
    QJsonObject describe() const;
    QString path() const { return m_path; }

    /// The value parsed and checked for `key`, or an invalid QVariant with `error` set.
    static QVariant parse(const Key &key, const QString &raw, QString *error);

private:
    QString save() const;
    QString m_path;
    QJsonObject m_values;
};

} // namespace uqbit

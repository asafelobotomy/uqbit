// The torrent engine: qBittorrent-nox (or qBittorrent Enhanced's nox) in a profile of uQbit's
// own (~/.local/share/uqbit), so it never shares settings with a qBittorrent the user runs
// themselves. Its Web API listens on 127.0.0.1 only, behind a random password that only this
// user can read (~/.local/state/uqbit/webui-password, mode 0600).
#pragma once

#include "config.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace uqbit {

/// The engine's WebUI user name.
inline constexpr auto kWebUser = "uqbit";
/// The systemd user unit that runs `uqbit engine run`.
inline constexpr auto kUnit = "uqbit-engine.service";

/// $UQBIT_NOX, else qbittorrent-nox on PATH; empty when there is none.
QString engineBinary();
/// <profile>/qBittorrent/config/qBittorrent.conf.
QString engineConfPath(const QString &profile = profileDir());
QString passwordPath();

/// The WebUI password, made (random, 0600) the first time. Empty with `error` set when the
/// file can't be read or written.
QString webPassword(QString *error = nullptr);

/// qBittorrent's stored form of a password: base64(salt):base64(PBKDF2-HMAC-SHA512, 100000
/// rounds, 64 bytes). `salt` empty = a new random one.
QByteArray hashPassword(const QString &password, QByteArray salt = {});
bool checkPassword(const QString &password, const QByteArray &stored);

/// Writes what the engine needs into its profile before it starts: the legal notice accepted,
/// the WebUI on 127.0.0.1:<webui_port> with uQbit's user and password hash. Leaves every other
/// setting alone. Returns an error message or "".
QString prepareProfile(const Config &config, const QString &profile = profileDir());

/// qBittorrent's encryption setting for uQbit's: 1 require, 0 prefer, 2 allow.
int encryptionMode(const Config &config);

/// The engine's command line (without the binary).
QStringList engineArgs(const Config &config, const QString &profile = profileDir());

/// Prepares the profile and replaces this process with the engine (for the systemd unit and
/// for a detached start). Returns only on failure, with an exit code.
int execEngine(const Config &config);

/// Whether systemd --user knows uQbit's unit (and UQBIT_NO_SYSTEMD isn't set).
bool haveUserUnit();

/// Starts the engine: the systemd user unit (sandboxed; its writable folders brought up to
/// date first) when there is one, else `uqbit engine run` detached. Doesn't wait for it to
/// answer. Returns an error message or "".
QString startEngine(const Config &config, const QString &self);

// ---- the sandbox (systemd unit only)

/// The folders the sandboxed engine may write: its profile, uQbit's state dir, the download
/// folder and every folder a torrent was saved to (remembered in <state>/folders).
QStringList writableFolders(const Config &config);
/// Whether `folder` is inside one of writableFolders().
bool folderAllowed(const Config &config, const QString &folder);
/// Remembers `folder` as writable. Returns false when it was already covered.
bool allowFolder(const Config &config, const QString &folder);
/// <config>/systemd/user/uqbit-engine.service.d/folders.conf.
QString dropInPath();
/// The drop-in's text: ReadWritePaths for each folder, quoted and escaped for systemd.
QString sandboxDropIn(const QStringList &folders);
/// Writes the drop-in (and makes the folders that should exist), then reloads systemd when
/// it changed. Returns an error message or "".
QString updateSandbox(const Config &config, bool *changed = nullptr);
/// Whether the engine runs as the systemd unit right now.
bool engineUnitActive();
/// Restarts the unit (after updateSandbox gave it a new folder). Returns an error or "".
QString restartEngineUnit();

/// `systemctl --user enable|disable` the unit. Returns an error message or "".
QString setStartOnLogin(bool on);

} // namespace uqbit

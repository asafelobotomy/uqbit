#include "engine.h"

#include "privacy.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPasswordDigestor>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace uqbit {

QString engineBinary()
{
    const QString env = qEnvironmentVariable("UQBIT_NOX");
    if (!env.isEmpty())
        return QFileInfo(env).isExecutable() ? env : QString();
    return QStandardPaths::findExecutable(QStringLiteral("qbittorrent-nox"));
}

QString engineConfPath(const QString &profile)
{
    return profile + "/qBittorrent/config/qBittorrent.conf";
}

QString passwordPath() { return stateDir() + "/webui-password"; }

QString webPassword(QString *error)
{
    const QString path = passwordPath();
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        const QString pw = QString::fromLatin1(f.read(256)).trimmed();
        if (pw.size() >= 16)
            return pw;
    }
    QByteArray raw(24, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(raw.data()), raw.size() / 4);
    const QString pw = QString::fromLatin1(
        raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    QDir().mkpath(stateDir());
    QFile::setPermissions(stateDir(), QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                          | QFileDevice::ExeOwner);
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("can't write %1: %2").arg(path, out.errorString());
        return {};
    }
    out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    out.write(pw.toLatin1() + '\n');
    if (!out.commit()) {
        if (error)
            *error = QStringLiteral("can't write %1: %2").arg(path, out.errorString());
        return {};
    }
    return pw;
}

QByteArray hashPassword(const QString &password, QByteArray salt)
{
    if (salt.isEmpty()) {
        salt.resize(16);
        QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(salt.data()), 4);
    }
    const QByteArray key = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha512, password.toUtf8(), salt, 100000, 64);
    return salt.toBase64() + ':' + key.toBase64();
}

bool checkPassword(const QString &password, const QByteArray &stored)
{
    const QList<QByteArray> parts = stored.split(':');
    if (parts.size() != 2)
        return false;
    const QByteArray salt = QByteArray::fromBase64(parts.at(0));
    return !salt.isEmpty() && hashPassword(password, salt) == stored;
}

QString prepareProfile(const Config &config, const QString &profile)
{
    QString error;
    const QString pw = webPassword(&error);
    if (pw.isEmpty())
        return error;
    const QString conf = engineConfPath(profile);
    if (!QDir().mkpath(QFileInfo(conf).absolutePath()))
        return QStringLiteral("can't make %1").arg(QFileInfo(conf).absolutePath());
    QFile::setPermissions(profile, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                       | QFileDevice::ExeOwner);

    QSettings s(conf, QSettings::IniFormat);
    auto put = [&s](const QString &key, const QVariant &v) {
        if (s.value(key) != v)
            s.setValue(key, v);
    };
    put("LegalNotice/Accepted", true);
    put("Preferences/WebUI/Enabled", true);
    put("Preferences/WebUI/Address", QStringLiteral("127.0.0.1"));
    put("Preferences/WebUI/Port", config.number("webui_port"));
    put("Preferences/WebUI/Username", QString::fromLatin1(kWebUser));
    put("Preferences/WebUI/LocalHostAuth", true);
    put("Preferences/WebUI/UseUPnP", false);
    put("Preferences/WebUI/HTTPS/Enabled", false);
    // A torrent client that stays in the background shouldn't also nag about itself.
    put("Preferences/General/StatusbarExternalIPDisplayed", false);
    if (!checkPassword(pw, s.value("Preferences/WebUI/Password_PBKDF2").toByteArray()))
        s.setValue("Preferences/WebUI/Password_PBKDF2", hashPassword(pw));
    // Privacy, before the engine makes a single connection (the window and the CLI set the
    // same through the Web API while it runs). With vpn_only the engine is bound to the VPN's
    // interface: while that is down there is nothing to send through, so nothing leaks.
    const VpnState vpn = vpnState(config);
    put("BitTorrent/Session/Interface", vpn.interface);
    put("BitTorrent/Session/InterfaceName", vpn.interface);
    put("BitTorrent/Session/InterfaceAddress", QString());
    put("BitTorrent/Session/Encryption", encryptionMode(config));
    put("BitTorrent/Session/AnonymousModeEnabled", config.boolean("anonymous_mode"));
    const QString list = config.boolean("blocklist") ? blocklistPath() : QString();
    put("BitTorrent/Session/IPFilteringEnabled", !list.isEmpty());
    put("BitTorrent/Session/TrackerFilteringEnabled", !list.isEmpty());
    if (!list.isEmpty())
        put("BitTorrent/Session/IPFilter", list);
    // A first run saves where uQbit says, until the window sets it from the settings.
    if (!s.contains("BitTorrent/Session/DefaultSavePath"))
        s.setValue("BitTorrent/Session/DefaultSavePath",
                   expandHome(config.text("download_folder")));
    s.sync();
    if (s.status() != QSettings::NoError)
        return QStringLiteral("can't write %1").arg(conf);
    QFile::setPermissions(conf, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return {};
}

int encryptionMode(const Config &config)
{
    const QString e = config.text("encryption");
    return e == "require" ? 1 : e == "allow" ? 2 : 0;
}

QStringList engineArgs(const Config &config, const QString &profile)
{
    return {QStringLiteral("--profile=") + profile, QStringLiteral("--confirm-legal-notice"),
            QStringLiteral("--webui-port=") + QString::number(config.number("webui_port"))};
}

int execEngine(const Config &config)
{
    const QString bin = engineBinary();
    if (bin.isEmpty()) {
        fprintf(stderr, "uqbit: qbittorrent-nox is not installed\n");
        return 3;
    }
    const QString e = prepareProfile(config);
    if (!e.isEmpty()) {
        fprintf(stderr, "uqbit: %s\n", qPrintable(e));
        return 1;
    }
    QList<QByteArray> args{QFile::encodeName(bin)};
    for (const QString &a : engineArgs(config))
        args.append(QFile::encodeName(a));
    std::vector<char *> argv;
    for (QByteArray &a : args)
        argv.push_back(a.data());
    argv.push_back(nullptr);
    ::execv(argv[0], argv.data());
    fprintf(stderr, "uqbit: can't start %s: %s\n", argv[0], std::strerror(errno));
    return 1;
}

static int systemctl(const QStringList &args, QString *out = nullptr)
{
    QProcess p;
    p.start(QStringLiteral("systemctl"), QStringList{"--user"} + args);
    if (!p.waitForFinished(15000)) {
        p.kill();
        return -1;
    }
    if (out)
        *out = QString::fromLocal8Bit(p.readAllStandardError()).trimmed();
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
}

bool haveUserUnit()
{
    if (qEnvironmentVariableIsSet("UQBIT_NO_SYSTEMD"))
        return false;
    if (QStandardPaths::findExecutable(QStringLiteral("systemctl")).isEmpty())
        return false;
    QProcess p;
    p.start(QStringLiteral("systemctl"),
            {"--user", "show", "-p", "LoadState", "--value", QString::fromLatin1(kUnit)});
    if (!p.waitForFinished(5000))
        return false;
    return p.exitCode() == 0 && p.readAllStandardOutput().trimmed() == "loaded";
}

QString startEngine(const Config &config, const QString &self)
{
    if (engineBinary().isEmpty())
        return QStringLiteral("qbittorrent-nox is not installed");
    if (haveUserUnit() && updateSandbox(config).isEmpty()) {
        QString err;
        if (systemctl({"start", QString::fromLatin1(kUnit)}, &err) == 0)
            return {};
        // Fall through: a broken user manager (or a kernel without the namespaces the
        // sandbox needs) shouldn't stop the app from working.
    }
    // Detached, with its output in a log of its own (not the caller's terminal or pipe).
    QDir().mkpath(stateDir());
    QProcess p;
    p.setProgram(self);
    p.setArguments({QStringLiteral("engine"), QStringLiteral("run")});
    p.setStandardInputFile(QProcess::nullDevice());
    p.setStandardOutputFile(stateDir() + "/engine.log");
    p.setStandardErrorFile(stateDir() + "/engine.log", QIODevice::Append);
    if (!p.startDetached())
        return QStringLiteral("can't start the torrent engine");
    return {};
}

// ---- the sandbox

static QString foldersPath() { return stateDir() + "/folders"; }

static QString clean(const QString &path)
{
    return QDir::cleanPath(QDir::isAbsolutePath(path) ? path : expandHome(path));
}

static QStringList remembered()
{
    QStringList out;
    QFile f(foldersPath());
    if (f.open(QIODevice::ReadOnly) && f.size() < 256 * 1024)
        for (const QByteArray &l : f.readAll().split('\n')) {
            const QString p = QString::fromUtf8(l).trimmed();
            if (QDir::isAbsolutePath(p) && !p.contains(QLatin1String("/../")))
                out << QDir::cleanPath(p);
        }
    return out;
}

QStringList writableFolders(const Config &config)
{
    QStringList out{profileDir(), stateDir(), clean(config.text("download_folder"))};
    for (const QString &p : remembered())
        if (!out.contains(p))
            out << p;
    return out;
}

static bool inside(const QString &folder, const QString &root)
{
    return folder == root || folder.startsWith(root.endsWith('/') ? root : root + '/');
}

bool folderAllowed(const Config &config, const QString &folder)
{
    const QString f = clean(folder);
    for (const QString &root : writableFolders(config))
        if (inside(f, root))
            return true;
    return false;
}

bool allowFolder(const Config &config, const QString &folder)
{
    if (folderAllowed(config, folder)
        || QRegularExpression("[\\x00-\\x1f]").match(folder).hasMatch())
        return false;
    QStringList list = remembered();
    list << clean(folder);
    QDir().mkpath(stateDir());
    QSaveFile out(foldersPath());
    if (!out.open(QIODevice::WriteOnly))
        return false;
    out.write(list.join('\n').toUtf8() + '\n');
    return out.commit();
}

QString dropInPath()
{
    const QString env = qEnvironmentVariable("XDG_CONFIG_HOME");
    const QString base = QDir::isAbsolutePath(env) ? env : QDir::homePath() + "/.config";
    return base + "/systemd/user/" + QString::fromLatin1(kUnit) + ".d/folders.conf";
}

QString sandboxDropIn(const QStringList &folders)
{
    QString out = QStringLiteral(
        "# Written by uQbit: the folders its sandboxed engine may write to. uQbit rewrites this\n"
        "# file when you add a torrent to a new folder; edit uQbit's settings instead.\n"
        "[Service]\n");
    for (QString p : folders) {
        // "-": a folder that doesn't exist (an unplugged drive) doesn't stop the engine.
        p.replace('\\', QLatin1String("\\\\")).replace('"', QLatin1String("\\\""));
        p.replace('%', QLatin1String("%%"));
        out += QStringLiteral("ReadWritePaths=\"-%1\"\n").arg(p);
    }
    return out;
}

QString updateSandbox(const Config &config, bool *changed)
{
    if (changed)
        *changed = false;
    const QStringList folders = writableFolders(config);
    for (int i = 0; i < 3 && i < folders.size(); ++i) // profile, state, downloads
        QDir().mkpath(folders.at(i));
    const QString text = sandboxDropIn(folders);
    const QString path = dropInPath();
    QFile old(path);
    if (old.open(QIODevice::ReadOnly) && old.readAll() == text.toUtf8())
        return {};
    old.close();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(text.toUtf8()) < 0 || !out.commit())
        return QStringLiteral("can't write %1").arg(path);
    if (changed)
        *changed = true;
    QString err;
    if (systemctl({"daemon-reload"}, &err) != 0)
        return err.isEmpty() ? QStringLiteral("systemctl daemon-reload failed") : err;
    return {};
}

bool engineUnitActive()
{
    return haveUserUnit()
           && systemctl({"is-active", "--quiet", QString::fromLatin1(kUnit)}) == 0;
}

QString restartEngineUnit()
{
    QString err;
    if (systemctl({"restart", QString::fromLatin1(kUnit)}, &err) != 0)
        return err.isEmpty() ? QStringLiteral("can't restart the torrent engine") : err;
    return {};
}

QString setStartOnLogin(bool on)
{
    if (!haveUserUnit())
        return QStringLiteral("starting on login needs systemd's user manager");
    QString err;
    if (on) {
        const QString e = updateSandbox(Config());
        if (!e.isEmpty())
            return e;
    }
    if (systemctl({on ? "enable" : "disable", QString::fromLatin1(kUnit)}, &err) != 0)
        return err.isEmpty() ? QStringLiteral("systemctl failed") : err;
    return {};
}

} // namespace uqbit

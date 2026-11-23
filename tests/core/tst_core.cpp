// Core tests: settings, the engine's profile and password, .torrent/magnet parsing, the
// torrent list's merging of sync/maindata, the default app. No window, no engine.
#include "client.h"
#include "config.h"
#include "defaultapp.h"
#include "engine.h"
#include "torrentfile.h"
#include "torrents.h"
#include "autostart.h"
#include "trackers.h"
#include "plugins.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace uqbit;

static QByteArray bencodeTorrent(bool multi)
{
    // d4:infod...ee with the keys in order, as a real .torrent has them.
    QByteArray info = "d";
    if (multi)
        info += "5:filesld6:lengthi100e4:pathl3:sub5:a.mkveed6:lengthi23e4:pathl5:b.srteee";
    else
        info += "6:lengthi4096e";
    info += "4:name" + QByteArray::number(multi ? 6 : 7) + ":" + (multi ? "Folder" : "one.iso");
    info += "12:piece lengthi16384e6:pieces20:aaaaaaaaaaaaaaaaaaaae";
    return "d4:info" + info + "e";
}

static QJsonObject md(const char *json)
{
    return QJsonDocument::fromJson(json).object();
}

class TstCore : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        qputenv("XDG_STATE_HOME", m_dir.filePath("state").toUtf8());
        qputenv("XDG_CONFIG_HOME", m_dir.filePath("config").toUtf8());
        qputenv("XDG_DATA_HOME", m_dir.filePath("data").toUtf8());
        qunsetenv("UQBIT_CONFIG");
        qunsetenv("UQBIT_PROFILE");
    }

    void configDefaultsAndValidation()
    {
        const QString path = m_dir.filePath("c/config.json");
        Config c(path);
        QCOMPARE(c.text("download_folder"), QString("~/Downloads"));
        QCOMPARE(c.boolean("keep_running"), true);
        QCOMPARE(c.number("webui_port"), 17845);
        QVERIFY(c.set("download_limit", "512").isEmpty());
        QVERIFY(!c.set("download_limit", "-1").isEmpty());
        QVERIFY(!c.set("download_limit", "1e3").isEmpty());
        QVERIFY(!c.set("seed_ratio", "101").isEmpty());
        QVERIFY(!c.set("webui_port", "80").isEmpty());
        QVERIFY(!c.set("download_folder", "relative/dir").isEmpty());
        QVERIFY(!c.set("download_folder", "~/a/../../etc").isEmpty());
        QVERIFY(c.set("download_folder", "~/Torrents").isEmpty());
        QVERIFY(c.set("ask_before_adding", "off").isEmpty());
        QVERIFY(!c.set("nope", "1").isEmpty());
        Config again(path);
        QCOMPARE(again.number("download_limit"), 512);
        QCOMPARE(again.text("download_folder"), QString("~/Torrents"));
        QCOMPARE(again.boolean("ask_before_adding"), false);
        QVERIFY(again.reset("download_limit").isEmpty());
        QCOMPARE(Config(path).number("download_limit"), 0);
        const QJsonObject d = again.describe();
        QCOMPARE(d.value("settings").toArray().size(), keys().size());
    }

    void configIgnoresJunk()
    {
        const QString path = m_dir.filePath("junk.json");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"download_limit": "abc", "upload_limit": 64, "keep_running": "no", "x": 1})");
        f.close();
        Config c(path);
        QCOMPARE(c.number("download_limit"), 0);
        QCOMPARE(c.number("upload_limit"), 64);
        QCOMPARE(c.boolean("keep_running"), false);
    }

    void passwordHashMatchesQbittorrent()
    {
        // qBittorrent's own form: base64(salt):base64(PBKDF2-HMAC-SHA512, 100000, 64 bytes).
        const QByteArray stored = hashPassword("secret123");
        const QList<QByteArray> parts = stored.split(':');
        QCOMPARE(parts.size(), 2);
        QCOMPARE(QByteArray::fromBase64(parts[0]).size(), 16);
        QCOMPARE(QByteArray::fromBase64(parts[1]).size(), 64);
        QVERIFY(checkPassword("secret123", stored));
        QVERIFY(!checkPassword("secret124", stored));
        QVERIFY(!checkPassword("secret123", "garbage"));
        // A known vector (salt "0123456789abcdef"), computed with Python's hashlib.
        QCOMPARE(hashPassword("adminadmin", "0123456789abcdef"),
                 QByteArray("MDEyMzQ1Njc4OWFiY2RlZg==:ZZS5EElovMoBVYDjMAaPI3BYalEsuNQNBCv4gjnI6KDYLC66zug9YBa5FIuP+3n/qFDM467FTFd4KkOdsg0Ugw=="));
    }

    void passwordFileIsPrivateAndStable()
    {
        const QString a = webPassword();
        QVERIFY(a.size() >= 16);
        QCOMPARE(webPassword(), a);
        QFile::Permissions p = QFile::permissions(passwordPath());
        QVERIFY(p & QFile::ReadOwner);
        QVERIFY(!(p & QFile::ReadGroup));
        QVERIFY(!(p & QFile::ReadOther));
    }

    void profileGetsWebUiSettingsAndKeepsOthers()
    {
        const QString profile = m_dir.filePath("profile");
        const QString conf = engineConfPath(profile);
        QDir().mkpath(QFileInfo(conf).absolutePath());
        {
            QSettings s(conf, QSettings::IniFormat);
            s.setValue("BitTorrent/Session/Port", 51413);
            s.setValue("Preferences/WebUI/Port", 8080);
        }
        Config c(m_dir.filePath("pc.json"));
        QVERIFY(c.set("webui_port", "18000").isEmpty());
        QCOMPARE(prepareProfile(c, profile), QString());
        QSettings s(conf, QSettings::IniFormat);
        QCOMPARE(s.value("BitTorrent/Session/Port").toInt(), 51413);
        QCOMPARE(s.value("Preferences/WebUI/Port").toInt(), 18000);
        QCOMPARE(s.value("Preferences/WebUI/Address").toString(), QString("127.0.0.1"));
        QCOMPARE(s.value("Preferences/WebUI/Username").toString(), QString("uqbit"));
        QCOMPARE(s.value("LegalNotice/Accepted").toBool(), true);
        const QByteArray hash = s.value("Preferences/WebUI/Password_PBKDF2").toByteArray();
        QVERIFY(checkPassword(webPassword(), hash));
        // A second run keeps the same hash (no rewrite on every start).
        QCOMPARE(prepareProfile(c, profile), QString());
        QSettings again(conf, QSettings::IniFormat);
        QCOMPARE(again.value("Preferences/WebUI/Password_PBKDF2").toByteArray(), hash);
        QVERIFY(engineArgs(c, profile).contains("--webui-port=18000"));
        QVERIFY(engineArgs(c, profile).contains("--profile=" + profile));
    }

    void preferences()
    {
        Config c(m_dir.filePath("prefs.json"));
        c.set("download_limit", "100");
        c.set("seed_ratio", "2");
        c.set("notify_finished", "false");
        const QJsonObject p = preferencesFor(c, "/usr/bin/uqbit");
        QCOMPARE(p.value("dl_limit").toInt(), 102400);
        QCOMPARE(p.value("up_limit").toInt(), 0);
        QCOMPARE(p.value("max_ratio_enabled").toBool(), true);
        QCOMPARE(p.value("max_ratio").toDouble(), 2.0);
        QCOMPARE(p.value("autorun_enabled").toBool(), false);
        QCOMPARE(p.value("autorun_program").toString(),
                 QString("\"/usr/bin/uqbit\" notify-finished \"%N\" \"%F\""));
        QVERIFY(p.value("save_path").toString().endsWith("/Downloads"));
        // Defaults: qBittorrent's own queue, no temp folder, nothing watched.
        QCOMPARE(p.value("queueing_enabled").toBool(), true);
        QCOMPARE(p.value("max_active_downloads").toInt(), 3);
        QCOMPARE(p.value("max_active_torrents").toInt(), 5);
        QCOMPARE(p.value("temp_path_enabled").toBool(), false);
        QVERIFY(!p.contains("temp_path"));
        QCOMPARE(p.value("scan_dirs").toObject(), QJsonObject());
        QCOMPARE(p.value("torrent_content_layout").toString(), QString("Original"));
        QCOMPARE(p.value("max_ratio_act").toInt(), 0);
        QCOMPARE(p.value("max_seeding_time_enabled").toBool(), false);
    }

    void trackerLists()
    {
        const QStringList t = parseTrackers("# best\nudp://tracker.example.org:1337/announce\n\n"
                                            "udp://tracker.example.org:1337/announce\n"
                                            "https://t.example.net/announce \r\n"
                                            "wss://w.example.com/announce\nftp://no.example/x\n"
                                            "not a tracker\nhttp://\nudp://bad host/announce\n");
        QCOMPARE(t, (QStringList{"udp://tracker.example.org:1337/announce",
                                 "https://t.example.net/announce", "wss://w.example.com/announce"}));
        Config c(m_dir.filePath("trackers.json"));
        QCOMPARE(trackerListUrls(c), QStringList());
        QVERIFY(!c.set("tracker_lists", "http://plain.example/list.txt").isEmpty());
        QVERIFY(!c.set("tracker_lists", "https://a.example/x\nhttps://b.example/y").isEmpty());
        QCOMPARE(c.set("tracker_lists", "https://a.example/best.txt https://b.example/all.txt https://a.example/best.txt"), QString());
        QCOMPARE(trackerListUrls(c), (QStringList{"https://a.example/best.txt", "https://b.example/all.txt"}));
        // Nothing stored yet: the engine adds nothing.
        QFile::remove(trackersPath());
        QVERIFY(trackersStale());
        QJsonObject p = preferencesFor(c, "/usr/bin/uqbit");
        QCOMPARE(p.value("add_trackers_enabled").toBool(), false);
        QVERIFY(storeTrackers(t));
        QVERIFY(!trackersStale());
        QCOMPARE(storedTrackers(), t);
        p = preferencesFor(c, "/usr/bin/uqbit");
        QCOMPARE(p.value("add_trackers_enabled").toBool(), true);
        QCOMPARE(p.value("add_trackers").toString(), t.join('\n'));
        // Lists turned off: the stored ones stop being added.
        c.reset("tracker_lists");
        QCOMPARE(preferencesFor(c, "/usr/bin/uqbit").value("add_trackers_enabled").toBool(), false);
    }

    void qbtPluginFolders()
    {
        // qBittorrent's native folder first, so qbit-plugin-dl's default target is found.
        const QStringList dirs = qbtPluginDirs();
        QCOMPARE(dirs.first(), m_dir.filePath("data") + "/qBittorrent/nova3/engines");
        QVERIFY(dirs.at(1).endsWith("/.var/app/org.qbittorrent.qBittorrent/data/qBittorrent/nova3/engines"));
        QVERIFY(validPluginFile("piratebay.py"));
        QVERIFY(validPluginFile("one337x.py"));
        for (const char *bad : {"__init__.py", "nova2.py", "helpers.py", "1337x.py", "a b.py",
                                "x.pyc", "../x.py", "x.txt"})
            QVERIFY2(!validPluginFile(bad), bad);

        const QString a = m_dir.filePath("plugA"), b = m_dir.filePath("plugB");
        QDir().mkpath(a);
        QDir().mkpath(b);
        auto write = [](const QString &path, const QByteArray &data) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(data);
        };
        write(a + "/glotorrents.py", "#VERSION: 1.7\nclass glotorrents: pass\n");
        write(a + "/nova2.py", "x");
        write(a + "/empty.py", "");
        write(a + "/readme.txt", "x");
        write(b + "/glotorrents.py", "#VERSION: 1.0\n");
        write(b + "/ettv.py", "#VERSION: 1.2\n");
        write(b + "/huge.py", QByteArray((1 << 20) + 1, '#'));
        // The engine's own folder is never a source, even when listed.
        const QString own = enginePluginDir();
        QDir().mkpath(own);
        write(own + "/eztv.py", "#VERSION: 1.24\n");
        const QList<QbtPlugin> found = qbtPlugins({a, b, own, m_dir.filePath("missing")});
        QStringList names;
        for (const QbtPlugin &p : found)
            names << p.name;
        QCOMPARE(names, (QStringList{"glotorrents", "ettv"}));
        QCOMPARE(found.first().path, a + "/glotorrents.py"); // the first folder wins
        QVERIFY(!found.first().stamp.isEmpty());
        QCOMPARE(importedPlugins(), QJsonObject());

        Config c(m_dir.filePath("plugins.json"));
        QCOMPARE(c.boolean("import_plugins"), true);
        QCOMPARE(c.set("import_plugins", "false"), QString());
        QCOMPARE(c.boolean("import_plugins"), false);
    }

    void launchOnLogin()
    {
        QVERIFY(autostartPath().startsWith(m_dir.filePath("config") + "/autostart/"));
        QVERIFY(!launchesOnLogin());
        QCOMPARE(setLaunchOnLogin(true, false), QString());
        QVERIFY(launchesOnLogin());
        QFile f(autostartPath());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray plain = f.readAll();
        f.close();
        QVERIFY(plain.contains("\nExec=uqbit\n"));
        QCOMPARE(setLaunchOnLogin(true, true), QString());
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(f.readAll().contains("\nExec=uqbit --hidden\n"));
        f.close();
        QCOMPARE(setLaunchOnLogin(false, true), QString());
        QVERIFY(!QFile::exists(autostartPath()));
        // Someone else's file of that name stays.
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[Desktop Entry]\nExec=other\n");
        f.close();
        QVERIFY(!setLaunchOnLogin(true, false).isEmpty());
        QCOMPARE(setLaunchOnLogin(false, false), QString());
        QVERIFY(QFile::exists(autostartPath()));
        QFile::remove(autostartPath());
    }

    void folderAndQueuePreferences()
    {
        Config c(m_dir.filePath("queue.json"));
        QCOMPARE(c.set("incomplete_folder", "~/Torrents/part"), QString());
        QCOMPARE(c.set("watch_folder", "/data/watch"), QString());
        QVERIFY(!c.set("watch_folder", "relative/path").isEmpty());
        QVERIFY(!c.set("incomplete_folder", "~/../etc").isEmpty());
        c.set("content_layout", "no_subfolder");
        c.set("incomplete_extension", "true");
        c.set("max_active_downloads", "0");
        c.set("max_active_uploads", "0");
        c.set("max_active_torrents", "0");
        c.set("seed_time", "120");
        c.set("when_seeded", "remove");
        QVERIFY(!c.set("when_seeded", "delete").isEmpty());
        QJsonObject p = preferencesFor(c, "/usr/bin/uqbit");
        QCOMPARE(p.value("temp_path_enabled").toBool(), true);
        QCOMPARE(p.value("temp_path").toString(), QDir::homePath() + "/Torrents/part");
        QCOMPARE(p.value("scan_dirs").toObject(), (QJsonObject{{"/data/watch", 1}}));
        QCOMPARE(p.value("torrent_content_layout").toString(), QString("NoSubfolder"));
        QCOMPARE(p.value("incomplete_files_ext").toBool(), true);
        // All limits off turns the queue off; 0 is the engine's -1.
        QCOMPARE(p.value("queueing_enabled").toBool(), false);
        QCOMPARE(p.value("max_active_downloads").toInt(), -1);
        QCOMPARE(p.value("max_seeding_time_enabled").toBool(), true);
        QCOMPARE(p.value("max_seeding_time").toInt(), 120);
        QCOMPARE(p.value("max_ratio_act").toInt(), 1);
        c.set("max_active_uploads", "2");
        p = preferencesFor(c, "/usr/bin/uqbit");
        QCOMPARE(p.value("queueing_enabled").toBool(), true);
        QCOMPARE(p.value("max_active_uploads").toInt(), 2);
        // Both folders become writable for the sandboxed engine.
        const QStringList w = writableFolders(c);
        QVERIFY(w.contains(QDir::homePath() + "/Torrents/part"));
        QVERIFY(w.contains("/data/watch"));
    }

    void torrentFiles()
    {
        const QJsonObject one = describeTorrent(bencodeTorrent(false));
        QVERIFY(one.value("ok").toBool());
        QCOMPARE(one.value("name").toString(), QString("one.iso"));
        QCOMPARE(one.value("size").toInteger(), 4096);
        QCOMPARE(one.value("file_count").toInt(), 1);
        const QJsonObject multi = describeTorrent(bencodeTorrent(true));
        QVERIFY(multi.value("ok").toBool());
        QCOMPARE(multi.value("name").toString(), QString("Folder"));
        QCOMPARE(multi.value("size").toInteger(), 123);
        QCOMPARE(multi.value("file_count").toInt(), 2);
        QCOMPARE(multi.value("files").toArray().at(0).toObject().value("path").toString(),
                 QString("sub/a.mkv"));
        QVERIFY(!describeTorrent("not bencode").value("ok").toBool());
        QVERIFY(!describeTorrent("d4:infoi3ee").value("ok").toBool());
        QVERIFY(!describeTorrent("d4:info" + QByteArray(100, 'l')).value("ok").toBool());
        QVERIFY(!describeTorrent("d4:infod4:name99999:xee").value("ok").toBool());
    }

    void magnets()
    {
        const QJsonObject m = describeMagnet(
            "magnet:?xt=urn:btih:08ADA5A7A6183AAE1E09D831DF6748D566095A10&dn=Sintel+2010&tr=udp%3A%2F%2Fa&tr=udp%3A%2F%2Fb");
        QVERIFY(m.value("ok").toBool());
        QCOMPARE(m.value("hash").toString(), QString("08ada5a7a6183aae1e09d831df6748d566095a10"));
        QCOMPARE(m.value("name").toString(), QString("Sintel 2010"));
        QCOMPARE(m.value("trackers").toInt(), 2);
        QVERIFY(!describeMagnet("magnet:?dn=x").value("ok").toBool());
        QVERIFY(!describeMagnet("https://example.org").value("ok").toBool());
        QVERIFY(isTorrentLink("magnet:?xt=urn:btih:abc"));
        QVERIFY(isTorrentLink("https://example.org/a/file.torrent"));
        QVERIFY(!isTorrentLink("https://example.org/a/file.iso"));
        QVERIFY(!isTorrentLink("/home/me/file.torrent"));
    }

    void stateGroups()
    {
        QCOMPARE(stateGroup("downloading"), QString("downloading"));
        QCOMPARE(stateGroup("stalledDL"), QString("downloading"));
        QCOMPARE(stateGroup("metaDL"), QString("downloading"));
        QCOMPARE(stateGroup("uploading"), QString("seeding"));
        QCOMPARE(stateGroup("stalledUP"), QString("seeding"));
        QCOMPARE(stateGroup("stoppedUP"), QString("stopped"));
        QCOMPARE(stateGroup("pausedDL"), QString("stopped"));
        QCOMPARE(stateGroup("checkingDL"), QString("checking"));
        QCOMPARE(stateGroup("missingFiles"), QString("error"));
    }

    void maindataMerges()
    {
        TorrentModel m;
        QSignalSpy finished(&m, &TorrentModel::finished);
        qint64 rid = m.apply(md(R"({"rid": 1, "full_update": true,
            "torrents": {"aa": {"name": "Alpha", "state": "downloading", "progress": 0.5, "added_on": 10},
                         "bb": {"name": "Beta", "state": "stalledUP", "progress": 1, "added_on": 20}},
            "server_state": {"dl_info_speed": 100}})"));
        QCOMPARE(rid, 1);
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(m.rows(), QStringList({"bb", "aa"})); // newest first
        QCOMPARE(m.server().value("dl_info_speed").toInt(), 100);
        QCOMPARE(m.counts().value("downloading").toInt(), 1);
        QCOMPARE(m.counts().value("seeding").toInt(), 1);

        // A partial update changes only what it names.
        rid = m.apply(md(R"({"rid": 2, "torrents": {"aa": {"progress": 1, "state": "uploading"}},
                             "server_state": {"up_info_speed": 5}})"));
        QCOMPARE(rid, 2);
        QCOMPARE(m.get("aa").value("name").toString(), QString("Alpha"));
        QCOMPARE(m.get("aa").value("group").toString(), QString("seeding"));
        QCOMPARE(m.server().value("dl_info_speed").toInt(), 100);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(1).toString(), QString("Alpha"));

        m.setFilter("downloading");
        QCOMPARE(m.rowCount(), 0);
        m.setFilter("finished");
        QCOMPARE(m.rowCount(), 2);
        m.setFilter("bogus");
        QCOMPARE(m.filter(), QString("all"));
        m.setSearch("alp");
        QCOMPARE(m.rows(), QStringList({"aa"}));
        m.setSearch("");

        m.apply(md(R"({"rid": 3, "torrents_removed": ["bb"]})"));
        QCOMPARE(m.rows(), QStringList({"aa"}));
        // A full update replaces everything.
        m.apply(md(R"({"rid": 4, "full_update": true, "torrents": {"cc": {"name": "C", "state": "stoppedDL"}}})"));
        QCOMPARE(m.rows(), QStringList({"cc"}));
        QCOMPARE(m.counts().value("stopped").toInt(), 1);
    }

    void defaultApp()
    {
        const QString user = m_dir.filePath("mimeapps.list");
        QFile f(user);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[Default Applications]\nx-scheme-handler/magnet=org.qbittorrent.qBittorrent.desktop;\n"
                "text/plain=org.kde.kate.desktop;\n");
        f.close();
        QJsonObject s = defaultStatus(user, {});
        QCOMPARE(s.value("default").toBool(), false);
        QCOMPARE(s.value("magnet").toString(), QString("org.qbittorrent.qBittorrent.desktop"));
        QCOMPARE(makeDefault(user), QString());
        s = defaultStatus(user, {});
        QCOMPARE(s.value("default").toBool(), true);
        QFile back(user);
        QVERIFY(back.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(back.readAll());
        QVERIFY(text.contains("text/plain=org.kde.kate.desktop;"));
        QVERIFY(text.contains("application/x-bittorrent=io.github.asafelobotomy.uqbit.desktop;"));
    }

private:
    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(TstCore)
#include "tst_core.moc"

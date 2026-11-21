// Privacy and sandbox tests: VPN detection and the kill switch's binding, the blocklist's
// formats, the engine's privacy settings, the sandbox drop-in. No window, no engine.
#include "client.h"
#include "config.h"
#include "engine.h"
#include "privacy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <zlib.h>

using namespace uqbit;

static QByteArray gzip(const QByteArray &in)
{
    z_stream z{};
    deflateInit2(&z, 6, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    QByteArray out(int(deflateBound(&z, uLong(in.size()))) + 32, Qt::Uninitialized);
    z.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    z.avail_in = uInt(in.size());
    z.next_out = reinterpret_cast<Bytef *>(out.data());
    z.avail_out = uInt(out.size());
    deflate(&z, Z_FINISH);
    out.resize(qsizetype(z.total_out));
    deflateEnd(&z);
    return out;
}

class TstPrivacy : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        qputenv("XDG_STATE_HOME", m_dir.filePath("state").toUtf8());
        qputenv("XDG_CONFIG_HOME", m_dir.filePath("config").toUtf8());
        qputenv("XDG_DATA_HOME", m_dir.filePath("data").toUtf8());
        qputenv("UQBIT_NO_SYSTEMD", "1");
        qunsetenv("UQBIT_CONFIG");
        qunsetenv("UQBIT_PROFILE");
        QDir(m_dir.filePath("state")).removeRecursively();
    }

    void picksTheVpn()
    {
        const QList<NetIf> ifs{{"lo", true, 772},         {"enp3s0", true, 1},
                               {"tailscale0", true, 65534}, {"tun0", false, 65534},
                               {"wg-home", true, 65534},  {"proton0", true, 65534}};
        QCOMPARE(pickVpn(ifs), QString("wg-home"));
        // NetworkManager's WireGuard device wins, in its order.
        QCOMPARE(pickVpn(ifs, {"proton0"}), QString("proton0"));
        // A down tunnel, Tailscale and plain Ethernet never count.
        QCOMPARE(pickVpn({{"tun0", false, 65534}, {"tailscale0", true, 65534}, {"eth0", true, 1}},
                         {"tailscale0"}),
                 QString());
        // No link layer counts even under an unknown name.
        QCOMPARE(pickVpn({{"enp3s0", true, 1}, {"corpnet", true, 65534}}), QString("corpnet"));
    }

    void killSwitchBinding()
    {
        Config c(m_dir.filePath("vpn.json"));
        const QString none, wg = "wg0";
        QVERIFY(!vpnState(c, &wg).required);
        QCOMPARE(vpnState(c, &wg).interface, QString());
        QVERIFY(c.set("vpn_only", "true").isEmpty());
        VpnState s = vpnState(c, &wg);
        QVERIFY(s.required && s.up);
        QCOMPARE(s.interface, QString("wg0"));
        // VPN down and none seen before: bound to an interface that doesn't exist.
        s = vpnState(c, &none);
        QVERIFY(s.required && !s.up);
        QCOMPARE(s.interface, QString(kNoVpn));
        // A fixed interface stays the binding whether or not it is up.
        QVERIFY(c.set("vpn_interface", "tun3").isEmpty());
        s = vpnState(c, &wg);
        QCOMPARE(s.interface, QString("tun3"));
        QVERIFY(!s.up);
        QVERIFY(!c.set("vpn_interface", "wg0; rm").isEmpty());
        QVERIFY(!c.set("vpn_interface", "averyveryverylongname").isEmpty());

        // What uQ's island reads.
        QJsonObject j = vpnJson(vpnState(c, &wg), "tun3", true);
        QCOMPARE(j.value("vpn").toObject().value("bound").toBool(), true);
        QCOMPARE(j.value("protected").toBool(), false); // bound, but tun3 is down
        c.reset("vpn_interface");
        j = vpnJson(vpnState(c, &wg), "wg0", true);
        QCOMPARE(j.value("protected").toBool(), true);
        QCOMPARE(j.value("vpn").toObject().value("interface").toString(), QString("wg0"));
        j = vpnJson(vpnState(c, &none), kNoVpn, true);
        QVERIFY(j.value("vpn").toObject().value("interface").isNull());
        QCOMPARE(j.value("protected").toBool(), false);
        c.set("vpn_only", "false");
        j = vpnJson(vpnState(c, &wg), "", true);
        QCOMPARE(j.value("vpn").toObject().value("required").toBool(), false);
        QCOMPARE(j.value("vpn").toObject().value("bound").toBool(), false);
    }

    void privacyDefaults()
    {
        Config c(m_dir.filePath("priv.json"));
        QCOMPARE(c.text("encryption"), QString("require"));
        QCOMPARE(c.boolean("anonymous_mode"), true);
        QCOMPARE(c.boolean("blocklist"), true);
        QCOMPARE(encryptionMode(c), 1);
        c.set("encryption", "allow");
        QCOMPARE(encryptionMode(c), 2);
        QVERIFY(!c.set("encryption", "maybe").isEmpty());
        QVERIFY(!c.set("blocklist_url", "http://insecure.example/list").isEmpty());
        QVERIFY(c.set("blocklist_url", "https://example.org/list.gz").isEmpty());
        const QJsonObject p = preferencesFor(c, "/usr/bin/uqbit");
        QCOMPARE(p.value("encryption").toInt(), 2);
        QCOMPARE(p.value("anonymous_mode").toBool(), true);
        QCOMPARE(p.value("ip_filter_enabled").toBool(), false); // nothing downloaded yet
    }

    void blocklistFormats()
    {
        QString error;
        // P2P, gzipped (as the default list comes).
        const QByteArray p2p = "# comment\n\"Some org\":1.2.3.0-1.2.3.255\nOther:5.6.7.8-5.6.7.9\n";
        QString path = storeBlocklist(gzip(p2p), &error);
        QVERIFY2(path.endsWith("/blocklist.p2p"), qPrintable(error));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), p2p);
        QCOMPARE(blocklistPath(), path);
        QVERIFY(!blocklistStale());
        // DAT replaces it.
        path = storeBlocklist("001.002.003.000 - 001.002.003.255 , 000 , Some org\n", &error);
        QVERIFY(path.endsWith("/blocklist.dat"));
        QVERIFY(!QFile::exists(m_dir.filePath("state/uqbit/blocklist.p2p")));
        QCOMPARE(blocklistPath(), path);
        // Anything else is refused, and the stored list stays.
        QVERIFY(storeBlocklist("<html>not found</html>", &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(storeBlocklist(QByteArray("\x1f\x8bgarbage", 9), &error).isEmpty());
        QCOMPARE(blocklistPath(), path);
        // The engine is told about it.
        Config c(m_dir.filePath("bl.json"));
        const QJsonObject p = preferencesFor(c, "/usr/bin/uqbit");
        QCOMPARE(p.value("ip_filter_enabled").toBool(), true);
        QCOMPARE(p.value("ip_filter_path").toString(), path);
        c.set("blocklist", "false");
        QCOMPARE(preferencesFor(c, "/usr/bin/uqbit").value("ip_filter_enabled").toBool(), false);
    }

    void profileGetsPrivacy()
    {
        const QString profile = m_dir.filePath("profile");
        Config c(m_dir.filePath("pp.json"));
        c.set("vpn_only", "true");
        c.set("vpn_interface", "wg7");
        c.set("anonymous_mode", "false");
        QString error;
        QVERIFY(!storeBlocklist("a:1.1.1.1-1.1.1.2\n", &error).isEmpty());
        QCOMPARE(prepareProfile(c, profile), QString());
        QSettings s(engineConfPath(profile), QSettings::IniFormat);
        QCOMPARE(s.value("BitTorrent/Session/Interface").toString(), QString("wg7"));
        QCOMPARE(s.value("BitTorrent/Session/InterfaceName").toString(), QString("wg7"));
        QCOMPARE(s.value("BitTorrent/Session/Encryption").toInt(), 1);
        QCOMPARE(s.value("BitTorrent/Session/AnonymousModeEnabled").toBool(), false);
        QCOMPARE(s.value("BitTorrent/Session/IPFilteringEnabled").toBool(), true);
        QCOMPARE(s.value("BitTorrent/Session/IPFilter").toString(), blocklistPath());
        c.set("vpn_only", "false");
        QCOMPARE(prepareProfile(c, profile), QString());
        QSettings again(engineConfPath(profile), QSettings::IniFormat);
        QCOMPARE(again.value("BitTorrent/Session/Interface").toString(), QString());
    }

    void sandboxFolders()
    {
        Config c(m_dir.filePath("sb.json"));
        c.set("download_folder", m_dir.filePath("dl"));
        QVERIFY(folderAllowed(c, m_dir.filePath("dl/sub")));
        QVERIFY(!folderAllowed(c, m_dir.filePath("dl2")));
        QVERIFY(!folderAllowed(c, m_dir.filePath("dl/../other")));
        QVERIFY(allowFolder(c, m_dir.filePath("Films 100%")));
        QVERIFY(!allowFolder(c, m_dir.filePath("Films 100%/sub"))); // already covered
        QVERIFY(!allowFolder(c, m_dir.filePath("bad\nname")));
        const QStringList w = writableFolders(c);
        QCOMPARE(w.size(), 4);
        QCOMPARE(w.at(0), profileDir());
        QCOMPARE(w.at(1), stateDir());
        QVERIFY(w.contains(m_dir.filePath("Films 100%")));

        const QString text = sandboxDropIn({"/home/a b/x", "/q\"uote\\d", "/p%c"});
        QVERIFY(text.contains("[Service]\n"));
        QVERIFY(text.contains("ReadWritePaths=\"-/home/a b/x\"\n"));
        QVERIFY(text.contains("ReadWritePaths=\"-/q\\\"uote\\\\d\"\n"));
        QVERIFY(text.contains("ReadWritePaths=\"-/p%%c\"\n"));
        QVERIFY(dropInPath().endsWith("/systemd/user/uqbit-engine.service.d/folders.conf"));
    }

    void privateRangesStayOpen()
    {
        int dropped = 0;
        const QByteArray p2p = "Yourself:127.0.0.1-127.0.0.1\n"
                               "RFC1918:192.168.0.0-192.168.255.255\n"
                               "Wide:9.0.0.0-11.0.0.0\n"
                               "Name: with colon:1.2.3.4-1.2.3.5\n"
                               "v6:2001:db8::1-2001:db8::ff\n"
                               "ula:fd00::1-fd00::2\n"
                               "# comment\n";
        const QByteArray out = dropPrivateRanges(p2p, "p2p", &dropped);
        QCOMPARE(dropped, 4);
        QCOMPARE(out, QByteArray("Name: with colon:1.2.3.4-1.2.3.5\n"
                                 "v6:2001:db8::1-2001:db8::ff\n# comment\n"));
        const QByteArray dat = "010.000.000.000 - 010.255.255.255 , 000 , lan\n"
                               "001.002.003.000 - 001.002.003.255 , 000 , ok\n";
        QCOMPARE(dropPrivateRanges(dat, "dat", &dropped),
                 QByteArray("001.002.003.000 - 001.002.003.255 , 000 , ok\n"));
        QCOMPARE(dropped, 1);
    }

    void gunzipLimits()
    {
        QString error;
        const QByteArray big(1 << 20, 'a');
        QCOMPARE(gunzip(gzip(big), 2 << 20, &error), big);
        QVERIFY(gunzip(gzip(big), 1000, &error).isEmpty());
        QVERIFY(error.contains("too big"));
        QVERIFY(gunzip(gzip(big).left(100), 2 << 20, &error).isEmpty());
    }

private:
    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(TstPrivacy)
#include "tst_privacy.moc"

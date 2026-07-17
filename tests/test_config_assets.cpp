// test_config_assets.cpp — assetKeys persistence round-trip.
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "ConfigStore.h"
#include "RuleEngine.h"   // IdleConfig

using namespace OmniPresence;

class TestConfigAssets : public QObject {
    Q_OBJECT
private slots:
    void assetKeysRoundTrip() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("c.json"));

        ConfigStore a;
        a.setConfigPathForTest(path);
        a.setAssetKey(QStringLiteral("osrs"), QStringLiteral("Playing OSRS"));
        a.setAssetKey(QStringLiteral("code"), QStringLiteral("Writing code"));
        QVERIFY(a.save());

        ConfigStore b;
        b.setConfigPathForTest(path);
        QVERIFY(b.load());
        QCOMPARE(b.assetKeys().value(QStringLiteral("osrs")), QStringLiteral("Playing OSRS"));
        QCOMPARE(b.assetKeys().value(QStringLiteral("code")), QStringLiteral("Writing code"));
    }

    // Phase 3 (Task 3.3): a fresh install (no config file yet) must still get
    // the shipped idle defaults — enabled=true, 120 s / 600 s thresholds, the
    // default labels, and the full raw.githubusercontent away icon URL.
    void idleDefaultsOnFreshInstall() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("fresh.json"));   // does not exist

        ConfigStore a;
        a.setConfigPathForTest(path);
        QVERIFY(a.load());

        const IdleConfig& idle = a.idleConfig();
        QCOMPARE(idle.enabled,      true);
        QCOMPARE(idle.afkSeconds,   quint64(120));
        QCOMPARE(idle.awaySeconds,  quint64(600));
        QCOMPARE(idle.afkLabel,     QStringLiteral("AFK"));
        QCOMPARE(idle.awayLabel,    QStringLiteral("Away from computer"));
        QCOMPARE(idle.awayImageKey, QStringLiteral(
            "https://raw.githubusercontent.com/jonathanavis96/OmniPresence/omnipresence-work/assets/icons/away.png"));
    }

    // A config file that omits the "idle" object entirely (or omits individual
    // fields within it) must also default rather than error/zero-out.
    void idlePartialConfigDefaultsMissingFields() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("partial.json"));

        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArrayLiteral(R"({
            "idle": { "awaySeconds": 300 }
        })"));
        f.close();

        ConfigStore a;
        a.setConfigPathForTest(path);
        QVERIFY(a.load());

        const IdleConfig& idle = a.idleConfig();
        QCOMPARE(idle.enabled,     true);          // defaulted — not present
        QCOMPARE(idle.afkSeconds,  quint64(120));   // defaulted — not present
        QCOMPARE(idle.awaySeconds, quint64(300));   // explicit value honoured
        QCOMPARE(idle.afkLabel,    QStringLiteral("AFK"));
        QCOMPARE(idle.awayLabel,   QStringLiteral("Away from computer"));
    }

    // GUI-persisted idle config round-trips through save()/load().
    void idleConfigRoundTrip() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("idle-rt.json"));

        ConfigStore a;
        a.setConfigPathForTest(path);
        IdleConfig& cfg = a.idleConfig();
        cfg.enabled     = false;
        cfg.afkSeconds  = 90;
        cfg.awaySeconds = 300;
        QVERIFY(a.save());

        ConfigStore b;
        b.setConfigPathForTest(path);
        QVERIFY(b.load());
        QCOMPARE(b.idleConfig().enabled,     false);
        QCOMPARE(b.idleConfig().afkSeconds,  quint64(90));
        QCOMPARE(b.idleConfig().awaySeconds, quint64(300));
    }
};

QTEST_MAIN(TestConfigAssets)
#include "test_config_assets.moc"

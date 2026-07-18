// test_config_assets.cpp — assetKeys persistence round-trip.
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "ConfigStore.h"
#include "RuleEngine.h"       // IdleConfig
#include "CustomOverride.h"   // CustomOverrideConfig

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

    // ── Custom override (the "Custom" tab) ──────────────────────────────────

    // A fresh install must leave the override OFF with no presets — an untouched
    // config never overrides normal rules.
    void customDefaultsOffOnFreshInstall() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        ConfigStore a;
        a.setConfigPathForTest(tmp.filePath(QStringLiteral("fresh-custom.json")));
        QVERIFY(a.load());

        QCOMPARE(a.customConfig().enabled, false);
        QVERIFY(a.customConfig().presets.isEmpty());
        QCOMPARE(a.customConfig().mode, CustomMode::Single);
    }

    // Presets, mode, interval and imageLibrary round-trip through save()/load().
    void customConfigRoundTrip() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("custom-rt.json"));

        ConfigStore a;
        a.setConfigPathForTest(path);
        CustomOverrideConfig& cfg = a.customConfig();
        cfg.enabled         = true;
        cfg.mode            = CustomMode::Cycle;
        cfg.intervalSeconds = 7;
        CustomPreset p1; p1.label = QStringLiteral("Hi"); p1.name = QStringLiteral("hello");
        p1.largeImageKey = QStringLiteral("https://files.catbox.moe/a.png"); p1.includeInCycle = true;
        CustomPreset p2; p2.label = QStringLiteral("Bye"); p2.name = QStringLiteral("goodbye");
        p2.includeInCycle = false;
        cfg.presets << p1 << p2;
        cfg.imageLibrary << CustomImageAsset{QStringLiteral("a.png"), QStringLiteral("https://files.catbox.moe/a.png")};
        QVERIFY(a.save());

        ConfigStore b;
        b.setConfigPathForTest(path);
        QVERIFY(b.load());
        const CustomOverrideConfig& r = b.customConfig();
        QCOMPARE(r.enabled,         true);
        QCOMPARE(r.mode,            CustomMode::Cycle);
        QCOMPARE(r.intervalSeconds, 7);
        QCOMPARE(r.presets.size(),  2);
        QCOMPARE(r.presets.at(0).name,           QStringLiteral("hello"));
        QCOMPARE(r.presets.at(0).largeImageKey,  QStringLiteral("https://files.catbox.moe/a.png"));
        QCOMPARE(r.presets.at(0).includeInCycle, true);
        QCOMPARE(r.presets.at(1).includeInCycle, false);
        // Presets saved without an id (pre-id configs) are backfilled a stable id
        // on load so async work / activeIndex can track them across reorders.
        QVERIFY(!r.presets.at(0).id.isEmpty());
        QVERIFY(!r.presets.at(1).id.isEmpty());
        QVERIFY(r.presets.at(0).id != r.presets.at(1).id);
        QCOMPARE(r.imageLibrary.size(),          1);
        QCOMPARE(r.imageLibrary.at(0).url,       QStringLiteral("https://files.catbox.moe/a.png"));
    }

    // intervalSeconds is clamped to a minimum of 1 on load (a 0/negative value in
    // config must not produce a zero-interval timer).
    void customIntervalClampedToMinimumOne() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("custom-interval.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArrayLiteral(R"({ "custom": { "intervalSeconds": 0 } })"));
        f.close();

        ConfigStore a;
        a.setConfigPathForTest(path);
        QVERIFY(a.load());
        QCOMPARE(a.customConfig().intervalSeconds, 1);
    }

    // resolve() maps a preset to a payload, but returns nullopt for a blank name
    // (Discord would render an empty name as the bare app name).
    void customResolveGuardsBlankName() {
        CustomOverrideConfig cfg;
        CustomPreset named;  named.name = QStringLiteral("hello");  named.details = QStringLiteral("d");
        CustomPreset blank;  blank.name = QStringLiteral("   ");    // whitespace only
        cfg.presets << named << blank;

        const auto ok = cfg.resolve(0);
        QVERIFY(ok.has_value());
        QCOMPARE(ok->name,    QStringLiteral("hello"));
        QCOMPARE(ok->details, QStringLiteral("d"));

        QVERIFY(!cfg.resolve(1).has_value());   // blank name → nullopt
        QVERIFY(!cfg.resolve(5).has_value());   // out of range → nullopt
    }

    // cycleIndices() returns only the includeInCycle presets, in list order.
    void customCycleIndicesSkipsExcluded() {
        CustomOverrideConfig cfg;
        CustomPreset a; a.name = QStringLiteral("a"); a.includeInCycle = true;
        CustomPreset b; b.name = QStringLiteral("b"); b.includeInCycle = false;
        CustomPreset c; c.name = QStringLiteral("c"); c.includeInCycle = true;
        cfg.presets << a << b << c;

        const QList<int> idx = cfg.cycleIndices();
        QCOMPARE(idx.size(), 2);
        QCOMPARE(idx.at(0), 0);
        QCOMPARE(idx.at(1), 2);
    }

    // A blank-name preset must be dropped from the cycle even when includeInCycle
    // is set — otherwise its frame resolves to nothing and the override blinks
    // back to normal rules (new presets start unnamed + included).
    void customCycleIndicesSkipsBlankName() {
        CustomOverrideConfig cfg;
        CustomPreset a;     a.name = QStringLiteral("a"); a.includeInCycle = true;
        CustomPreset blank; blank.name = QStringLiteral("  "); blank.includeInCycle = true;
        CustomPreset c;     c.name = QStringLiteral("c"); c.includeInCycle = true;
        cfg.presets << a << blank << c;

        const QList<int> idx = cfg.cycleIndices();
        QCOMPARE(idx.size(), 2);
        QCOMPARE(idx.at(0), 0);
        QCOMPARE(idx.at(1), 2);   // the blank preset at 1 is skipped
    }
};

QTEST_MAIN(TestConfigAssets)
#include "test_config_assets.moc"

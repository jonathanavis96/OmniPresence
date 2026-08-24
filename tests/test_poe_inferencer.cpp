// test_poe_inferencer.cpp — PoeActivityInferencer: parsing + session
// inference, including the privacy guarantee around chat lines and the
// "never guess the local player's name" rule from the design.
//
// NOTE on evidence: every case here is fed synthetic log lines built to match
// the exact line forms documented in
// docs/superpowers/specs/2026-08-11-path-of-exile-integration-design.md
// (verified there against a real 18 MB Client.txt). This test does NOT run
// against a live-captured PoE log — no Path of Exile client is available in
// this environment — so treat the synthetic-input coverage below as exactly
// that: format-accurate synthetic input, not a captured-log run.
#include <QtTest>
#include <QFile>
#include <QTextStream>
#include "PoeActivityInferencer.h"
#include "PoeZoneTable.h"

using namespace OmniPresence;

#ifndef OMNI_SOURCE_DIR
#define OMNI_SOURCE_DIR "."
#endif

namespace {

PoeZoneTable& sharedZoneTable() {
    static PoeZoneTable table = [] {
        PoeZoneTable t;
        t.loadFromFile(QStringLiteral(OMNI_SOURCE_DIR "/config/poe-zones.json"));
        return t;
    }();
    return table;
}

} // namespace

class TestPoeInferencer : public QObject {
    Q_OBJECT
private slots:
    void zoneChangeUpdatesZoneAndCategory() {
        PoeActivityInferencer inf;
        inf.setZoneTable(&sharedZoneTable());
        inf.processLine(QStringLiteral(
            "2026/08/11 18:50:59 24067703 cffb065b [INFO Client 7272] : You have entered Cartographer's Hideout."));

        const PoeContext& ctx = inf.context();
        QCOMPARE(ctx.zone,         QStringLiteral("Cartographer's Hideout"));
        QCOMPARE(ctx.zoneCategory, QStringLiteral("hideout"));
        QCOMPARE(ctx.activity,     QStringLiteral("In Hideout"));
        QVERIFY(ctx.zoneEnteredAt.isValid());
        QVERIFY(ctx.sessionActive);
    }

    void zoneClassificationDegradesToUnknownWithNoZoneTable() {
        PoeActivityInferencer inf;   // no setZoneTable() call
        inf.processLine(QStringLiteral(
            "2026/08/11 18:50:59 1 aaa [INFO Client 1] : You have entered Some New Zone."));
        QCOMPARE(inf.context().zoneCategory, QStringLiteral("unknown"));
    }

    // Level-up lines fire for every player in the area/party, not just the
    // local one (design: 15 distinct names observed in one real session).
    // Only a name in the configured character list may update state.
    void levelUpOnlyUpdatesConfiguredCharacter() {
        PoeActivityInferencer inf;
        inf.setConfiguredCharacters({QStringLiteral("TestCharacter")});

        inf.processLine(QStringLiteral(
            "2026/08/11 18:51:20 1 aaa [INFO Client 1] : Wanderer1 (Witch) is now level 12"));
        QVERIFY(inf.context().character.isEmpty());   // not configured — ignored
        QCOMPARE(inf.context().level, 0);

        inf.processLine(QStringLiteral(
            "2026/08/11 18:51:25 2 aaa [INFO Client 1] : TestCharacter (Occultist) is now level 92"));
        QCOMPARE(inf.context().character,      QStringLiteral("TestCharacter"));
        QCOMPARE(inf.context().characterClass, QStringLiteral("Occultist"));
        QCOMPARE(inf.context().level,          92);
    }

    // The design explicitly rejects a "most frequent name this session"
    // heuristic. Prove it: an unconfigured name that levels up repeatedly
    // (more often than the configured one) must never be adopted as the
    // player.
    void mostFrequentUnconfiguredNameIsNeverAdoptedAsPlayer() {
        PoeActivityInferencer inf;
        inf.setConfiguredCharacters({QStringLiteral("Real Player")});

        for (int i = 0; i < 10; ++i) {
            inf.processLine(QStringLiteral(
                "2026/08/11 18:5%1:00 1 aaa [INFO Client 1] : Frequent%2 (Witch) is now level %3")
                .arg(i % 6).arg(i).arg(i + 1));
        }
        QVERIFY(inf.context().character.isEmpty());

        inf.processLine(QStringLiteral(
            "2026/08/11 18:59:00 2 aaa [INFO Client 1] : Real Player (Occultist) is now level 5"));
        QCOMPARE(inf.context().character, QStringLiteral("Real Player"));
    }

    // With no configured character at all, character/class/level/deaths stay
    // empty/zero — the zone-based presence alone still works.
    void noConfiguredCharactersOmitsCharacterFieldsEntirely() {
        PoeActivityInferencer inf;
        inf.setZoneTable(&sharedZoneTable());
        inf.processLine(QStringLiteral(
            "2026/08/11 18:51:25 1 aaa [INFO Client 1] : Someone (Ranger) is now level 40"));
        inf.processLine(QStringLiteral(
            "2026/08/11 18:52:00 2 aaa [INFO Client 1] : Someone has been slain."));
        inf.processLine(QStringLiteral(
            "2026/08/11 18:52:03 3 aaa [INFO Client 1] : You have entered Dunes."));

        const PoeContext& ctx = inf.context();
        QVERIFY(ctx.character.isEmpty());
        QVERIFY(ctx.characterClass.isEmpty());
        QCOMPARE(ctx.level,  0);
        QCOMPARE(ctx.deaths, 0);
        QCOMPARE(ctx.zone,   QStringLiteral("Dunes"));   // zone tracking is unaffected
    }

    // Deaths fire for every player in the area too (design: 49 distinct names
    // in one real session) — only the configured character's deaths count.
    void deathOnlyCountsForConfiguredCharacter() {
        PoeActivityInferencer inf;
        inf.setConfiguredCharacters({QStringLiteral("TestCharacter")});

        inf.processLine(QStringLiteral("2026/08/11 18:53:00 1 aaa [INFO Client 1] : Wanderer1 has been slain."));
        QCOMPARE(inf.context().deaths, 0);

        inf.processLine(QStringLiteral("2026/08/11 18:53:05 2 aaa [INFO Client 1] : TestCharacter has been slain."));
        QCOMPARE(inf.context().deaths, 1);

        inf.processLine(QStringLiteral("2026/08/11 18:54:00 3 aaa [INFO Client 1] : Someone Else has been slain."));
        QCOMPARE(inf.context().deaths, 1);   // still just the one, from TestCharacter
    }

    // The privacy guarantee: chat/whisper lines must never be parsed and must
    // produce no state change whatsoever, even when they contain text that
    // superficially resembles a system line (a whisper crafted to say
    // "You have entered X." must not spoof a zone change).
    void chatLinesProduceNoStateChangeAndAreNeverParsed() {
        PoeActivityInferencer inf;
        inf.setZoneTable(&sharedZoneTable());
        inf.processLine(QStringLiteral(
            "2026/08/11 18:51:10 1 aaa [INFO Client 1] : You have entered Lioneye's Watch."));
        const PoeContext before = inf.context();

        inf.processLine(QStringLiteral(
            "2026/08/11 18:51:11 2 aaa [INFO Client 1] @From <Guild> Sneaky: gl hf"));
        inf.processLine(QStringLiteral(
            "2026/08/11 18:51:12 3 aaa [INFO Client 1] @To Sneaky: gl"));
        // Adversarial: a whisper whose TEXT happens to look like a system line.
        // Real chat lines carry the @From/@To marker ahead of any message text,
        // so the privacy short-circuit must fire before pattern matching does.
        inf.processLine(QStringLiteral(
            "2026/08/11 18:51:13 4 aaa [INFO Client 1] @From Sneaky: You have entered Trolled Zone."));

        const PoeContext after = inf.context();
        QCOMPARE(after.zone,          before.zone);
        QCOMPARE(after.zoneCategory,  before.zoneCategory);
        QCOMPARE(after.character,     before.character);
        QCOMPARE(after.deaths,        before.deaths);
    }

    void focusAndAfkTogglesUpdateContext() {
        PoeActivityInferencer inf;
        QVERIFY(!inf.context().focused);
        QVERIFY(!inf.context().afk);

        inf.processLine(QStringLiteral("2026/08/11 18:51:02 1 aaa [INFO Client 1] [WINDOW] Gained focus"));
        QVERIFY(inf.context().focused);

        inf.processLine(QStringLiteral("2026/08/11 18:53:30 2 aaa [INFO Client 1] : AFK mode is now ON."));
        QVERIFY(inf.context().afk);

        inf.processLine(QStringLiteral("2026/08/11 18:54:00 3 aaa [INFO Client 1] : AFK mode is now OFF."));
        QVERIFY(!inf.context().afk);

        inf.processLine(QStringLiteral("2026/08/11 18:54:10 4 aaa [INFO Client 1] [WINDOW] Lost focus"));
        QVERIFY(!inf.context().focused);
    }

    // LOG FILE OPENING resets session stats (deaths, zone, character) but
    // must not forget the standing configured-character list — that is a
    // config setting, not session state.
    void logFileOpeningResetsSessionButKeepsConfiguredCharacters() {
        PoeActivityInferencer inf;
        inf.setZoneTable(&sharedZoneTable());
        inf.setConfiguredCharacters({QStringLiteral("TestCharacter")});

        inf.processLine(QStringLiteral("2026/08/11 18:51:25 1 aaa [INFO Client 1] : TestCharacter (Occultist) is now level 92"));
        inf.processLine(QStringLiteral("2026/08/11 18:53:05 2 aaa [INFO Client 1] : TestCharacter has been slain."));
        inf.processLine(QStringLiteral("2026/08/11 18:52:03 3 aaa [INFO Client 1] : You have entered Dunes."));
        QCOMPARE(inf.context().deaths, 1);
        QCOMPARE(inf.context().level,  92);

        inf.processLine(QStringLiteral("2026/08/11 19:00:00 4 bbb ***** LOG FILE OPENING *****"));
        const PoeContext& ctx = inf.context();
        QCOMPARE(ctx.deaths, 0);
        QCOMPARE(ctx.level,  0);
        QVERIFY(ctx.zone.isEmpty());
        QVERIFY(ctx.sessionActive);   // reset, not "nothing has happened yet"

        // Configured-character list survived the reset.
        inf.processLine(QStringLiteral("2026/08/11 19:00:05 5 bbb [INFO Client 1] : TestCharacter has been slain."));
        QCOMPARE(inf.context().deaths, 1);
    }

    // End-to-end pass over a realistic (synthetic, see file header) session:
    // a full sequence of zone/level/death/focus/AFK/chat lines, checking the
    // final resolved state rather than each intermediate step.
    void fixtureSessionResolvesExpectedFinalState() {
        QFile f(QStringLiteral(OMNI_SOURCE_DIR "/tests/fixtures/poe-latest-client-sample.txt"));
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QTextStream in(&f);

        PoeActivityInferencer inf;
        inf.setZoneTable(&sharedZoneTable());
        inf.setConfiguredCharacters({QStringLiteral("TestCharacter")});

        int lineCount = 0;
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
            inf.processLine(line);
            ++lineCount;
        }
        QVERIFY(lineCount > 10);   // sanity: the fixture actually has content

        const PoeContext& ctx = inf.context();
        QCOMPARE(ctx.zone,            QStringLiteral("Cartographer's Hideout"));
        QCOMPARE(ctx.zoneCategory,    QStringLiteral("hideout"));
        QCOMPARE(ctx.character,       QStringLiteral("TestCharacter"));
        QCOMPARE(ctx.characterClass,  QStringLiteral("Occultist"));
        QCOMPARE(ctx.level,           92);
        QCOMPARE(ctx.deaths,          1);     // only TestCharacter's death, not Wanderer1's
        QVERIFY(!ctx.afk);                    // ended with AFK OFF
        QVERIFY(!ctx.focused);                // ended with Lost focus
    }
};

QTEST_MAIN(TestPoeInferencer)
#include "test_poe_inferencer.moc"

// test_poe_zones.cpp — PoeZoneTable: shipped zones classify, pattern order,
// unknown degrades, ambiguous zones still resolve.
#include <QtTest>
#include <QPair>
#include <QList>
#include "PoeZoneTable.h"

using namespace OmniPresence;

#ifndef OMNI_SOURCE_DIR
#define OMNI_SOURCE_DIR "."
#endif

namespace {
QString shippedTablePath() {
    return QStringLiteral(OMNI_SOURCE_DIR "/config/poe-zones.json");
}
}

class TestPoeZones : public QObject {
    Q_OBJECT
private slots:
    void loadsTheShippedTable() {
        PoeZoneTable table;
        QVERIFY(table.loadFromFile(shippedTablePath()));
        QVERIFY(table.patternCount() > 0);
        QVERIFY(table.exactCount() > 0);
    }

    // One representative zone per category from the design doc's own count
    // table (docs/superpowers/specs/2026-08-11-path-of-exile-integration-design.md)
    // must resolve to something other than "unknown".
    void representativeZonesFromEveryCategoryClassify() {
        PoeZoneTable table;
        QVERIFY(table.loadFromFile(shippedTablePath()));

        static const QList<QPair<QString, QString>> kExpected = {
            {QStringLiteral("Cartographer's Hideout"), QStringLiteral("hideout")},
            {QStringLiteral("Lioneye's Watch"),         QStringLiteral("town")},
            {QStringLiteral("Dunes"),                   QStringLiteral("map")},
            {QStringLiteral("Toxic Sewer"),              QStringLiteral("campaign")},
            {QStringLiteral("Azurite Mine"),             QStringLiteral("delve")},
            {QStringLiteral("Aspirant's Trial"),         QStringLiteral("labyrinth")},
            {QStringLiteral("The Forbidden Sanctum"),    QStringLiteral("sanctum")},
            {QStringLiteral("The Rogue Harbour"),        QStringLiteral("town")},
            {QStringLiteral("The Menagerie"),            QStringLiteral("menagerie")},
            {QStringLiteral("Eye of the Storm"),         QStringLiteral("boss")},
            {QStringLiteral("Altered Distant Memory"),   QStringLiteral("memory")},
            {QStringLiteral("Hysteriagate"),             QStringLiteral("simulacrum")},
            {QStringLiteral("Monastery of the Keepers"), QStringLiteral("other")},
        };
        for (const auto& [zone, expected] : kExpected) {
            QCOMPARE(table.classify(zone), expected);
        }
    }

    // "The Forbidden Sanctum" and "The Rogue Harbour" have specific patterns
    // that must be checked BEFORE the generic "^The " -> campaign default —
    // this is the ordering the whole pattern list depends on.
    void patternsAreOrderedMostSpecificFirst() {
        PoeZoneTable table;
        QVERIFY(table.loadFromFile(shippedTablePath()));
        QCOMPARE(table.classify(QStringLiteral("The Forbidden Sanctum")), QStringLiteral("sanctum"));
        QCOMPARE(table.classify(QStringLiteral("The Rogue Harbour")),     QStringLiteral("town"));
        // No specific pattern/exact entry for this one — falls through to the
        // generic "The " campaign default.
        QCOMPARE(table.classify(QStringLiteral("The Twilight Strand")), QStringLiteral("campaign"));
    }

    void unknownZoneDegradesGracefully() {
        PoeZoneTable table;
        QVERIFY(table.loadFromFile(shippedTablePath()));
        QCOMPARE(table.classify(QStringLiteral("Some Unreleased League Zone")), QStringLiteral("unknown"));
    }

    // Dunes/Strand/Jungle Valley are reused for both a campaign zone and an
    // Atlas map — the design accepts classifying them as "map" (the more
    // likely endgame meaning) while flagging them ambiguous for callers that
    // care about the caveat.
    void ambiguousZonesAreFlaggedButStillClassified() {
        PoeZoneTable table;
        QVERIFY(table.loadFromFile(shippedTablePath()));
        QVERIFY(table.isAmbiguous(QStringLiteral("Dunes")));
        QVERIFY(table.isAmbiguous(QStringLiteral("Strand")));
        QVERIFY(table.isAmbiguous(QStringLiteral("Jungle Valley")));
        QCOMPARE(table.classify(QStringLiteral("Dunes")), QStringLiteral("map"));
        QVERIFY(!table.isAmbiguous(QStringLiteral("Lioneye's Watch")));
    }

    void missingFileLeavesTableUnchangedAndReturnsFalse() {
        PoeZoneTable table;
        QVERIFY(table.loadFromFile(shippedTablePath()));
        const int patterns = table.patternCount();
        QVERIFY(!table.loadFromFile(QStringLiteral("/nonexistent/path/poe-zones.json")));
        QCOMPARE(table.patternCount(), patterns);   // untouched, not cleared
    }

    void malformedJsonDegradesToUnknownRatherThanCrashing() {
        PoeZoneTable table;
        table.loadFromJson(QJsonObject());   // no patterns/exact/ambiguous at all
        QCOMPARE(table.classify(QStringLiteral("Anything")), QStringLiteral("unknown"));
        QVERIFY(!table.isAmbiguous(QStringLiteral("Anything")));
    }
};

QTEST_MAIN(TestPoeZones)
#include "test_poe_zones.moc"

// test_rule_engine_render.cpp — combined main line + Name display.
#include <QtTest>
#include <QJsonObject>
#include "RuleEngine.h"
#include "IntegrationContext.h"
#include "Rule.h"
#include "WindowInfo.h"
#include "PresencePayload.h"

using namespace OmniPresence;

static Rule runeLightRule() {
    Rule r;
    r.id                     = QStringLiteral("rl");
    r.name                   = QStringLiteral("RuneLight");
    r.enabled                = true;
    r.priority               = 10;
    r.matchProcessName       = QStringLiteral("RuneLite.exe");
    r.matchIntegrationSource = QStringLiteral("runelite");
    r.activityType           = ActivityType::Playing;
    r.activityNameTemplate   = QStringLiteral("RuneLight – {{runelite.activity}}");
    r.detailsTemplate        = QString();
    r.stateTemplate          = QStringLiteral("{{runelite.location}}");
    r.largeImageKey          = QStringLiteral("osrs");
    r.privacyLevel           = PrivacyLevel::Public;
    return r;
}

// Phase 2 (Task 2.2): the RuneLite rule as shipped in config/omnipresence.json
// — constant "OSRS" name (never a dangling "OSRS – "), skill-labelled details,
// bare location state.
static Rule osrsRule() {
    Rule r;
    r.id                     = QStringLiteral("osrs");
    r.name                   = QStringLiteral("RuneLite / OSRS");
    r.enabled                = true;
    r.priority               = 10;
    r.matchProcessName       = QStringLiteral("RuneLite.exe");
    r.matchIntegrationSource = QStringLiteral("runelite");
    r.activityType           = ActivityType::Playing;
    r.activityNameTemplate   = QStringLiteral("OSRS");
    r.detailsTemplate        = QStringLiteral("{{runelite.activity}}");
    r.stateTemplate          = QStringLiteral("{{runelite.location}}");
    r.largeImageKey          = QStringLiteral("osrs");
    r.privacyLevel           = PrivacyLevel::Public;
    return r;
}

static Rule terminalRule() {
    Rule r;
    r.id                     = QStringLiteral("term");
    r.name                   = QStringLiteral("Coding");
    r.enabled                = true;
    r.priority               = 20;
    r.matchProcessName       = QStringLiteral("WindowsTerminal.exe");
    r.matchIntegrationSource = QStringLiteral("terminal");
    r.activityType           = ActivityType::Playing;
    r.activityNameTemplate   = QStringLiteral("Coding – {{terminal.title or window.title}}");
    r.largeImageKey          = QStringLiteral("code");
    r.privacyLevel           = PrivacyLevel::Public;
    return r;
}

class TestRuleEngineRender : public QObject {
    Q_OBJECT
private slots:
    void combinedMainLineAndNameDisplay() {
        RuleSet rules;
        rules.addRule(runeLightRule());

        IntegrationContext integ;
        integ.update(QStringLiteral("runelite"), QJsonObject{
            {QStringLiteral("activity"), QStringLiteral("Training Crafting")},
            {QStringLiteral("location"), QStringLiteral("Grand Exchange")},
        });

        WindowInfo win;
        win.processName = QStringLiteral("runelite.exe");
        win.windowTitle = QStringLiteral("RuneLite");

        RuleEngine engine;
        ManualOverrideState override;
        PresencePayload prev;
        const PresencePayload p = engine.evaluate(win, integ, rules, override, prev);

        QCOMPARE(p.name,  QStringLiteral("RuneLight – Training Crafting"));
        QCOMPARE(p.state, QStringLiteral("Grand Exchange"));
        QCOMPARE(int(p.statusDisplay), int(StatusDisplay::Name));
    }

    void emptyActivityDropsDanglingSeparator() {
        RuleSet rules;
        rules.addRule(runeLightRule());

        IntegrationContext integ;   // fresh runelite payload, but no activity yet
        integ.update(QStringLiteral("runelite"), QJsonObject{
            {QStringLiteral("activity"), QString()},
            {QStringLiteral("location"), QString()},
        });

        WindowInfo win;
        win.processName = QStringLiteral("runelite.exe");

        RuleEngine engine;
        ManualOverrideState override;
        PresencePayload prev;
        const PresencePayload p = engine.evaluate(win, integ, rules, override, prev);

        QCOMPARE(p.name, QStringLiteral("RuneLight"));   // not "RuneLight – "
    }

    void osrsSkillLabelledDetailsAndConstantName() {
        RuleSet rules;
        rules.addRule(osrsRule());

        IntegrationContext integ;
        integ.update(QStringLiteral("runelite"), QJsonObject{
            {QStringLiteral("activity"), QStringLiteral("Runecraft")},
            {QStringLiteral("location"), QStringLiteral("Dark Altar")},
        });

        WindowInfo win;
        win.processName = QStringLiteral("runelite.exe");
        win.windowTitle = QStringLiteral("RuneLite");

        RuleEngine engine;
        ManualOverrideState override;
        PresencePayload prev;
        const PresencePayload p = engine.evaluate(win, integ, rules, override, prev);

        QCOMPARE(p.name,    QStringLiteral("OSRS"));
        QCOMPARE(p.details, QStringLiteral("Training Runecrafting"));
        QCOMPARE(p.state,   QStringLiteral("Dark Altar"));
    }

    void osrsEmptyActivityAndLocationNeverDangles() {
        RuleSet rules;
        rules.addRule(osrsRule());

        IntegrationContext integ;   // fresh runelite payload, but no activity/location yet
        integ.update(QStringLiteral("runelite"), QJsonObject{
            {QStringLiteral("activity"), QString()},
            {QStringLiteral("location"), QString()},
        });

        WindowInfo win;
        win.processName = QStringLiteral("runelite.exe");
        win.windowTitle = QStringLiteral("RuneLite");

        RuleEngine engine;
        ManualOverrideState override;
        PresencePayload prev;
        const PresencePayload p = engine.evaluate(win, integ, rules, override, prev);

        QCOMPARE(p.name, QStringLiteral("OSRS"));   // not "OSRS –" / "OSRS - "
        QVERIFY(!p.name.contains(QLatin1Char('–')));
        QVERIFY(!p.name.contains(QLatin1Char('-')));
    }

    void terminalMainLineUsesTabTitle() {
        RuleSet rules; rules.addRule(terminalRule());
        IntegrationContext integ;
        integ.update(QStringLiteral("terminal"), QJsonObject{
            {QStringLiteral("repo"), QString()},
        });
        WindowInfo win;
        win.processName = QStringLiteral("windowsterminal.exe");
        win.windowTitle = QStringLiteral("RAM");
        RuleEngine engine; ManualOverrideState ov; PresencePayload prev;
        const PresencePayload p = engine.evaluate(win, integ, rules, ov, prev);
        QCOMPARE(p.name, QStringLiteral("Coding – RAM"));
    }

    void terminalNoTitleDropsSeparator() {
        RuleSet rules; rules.addRule(terminalRule());
        IntegrationContext integ;
        integ.update(QStringLiteral("terminal"), QJsonObject{
            {QStringLiteral("repo"), QString()},
        });
        WindowInfo win;
        win.processName = QStringLiteral("windowsterminal.exe");   // no windowTitle
        RuleEngine engine; ManualOverrideState ov; PresencePayload prev;
        const PresencePayload p = engine.evaluate(win, integ, rules, ov, prev);
        QCOMPARE(p.name, QStringLiteral("Coding"));
    }
};

QTEST_MAIN(TestRuleEngineRender)
#include "test_rule_engine_render.moc"

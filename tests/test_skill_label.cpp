// test_skill_label.cpp — omni::skillLabel skill-name -> "Training {Skill}" mapping.
#include <QtTest>
#include "SkillLabel.h"

using namespace omni;

class TestSkillLabel : public QObject {
    Q_OBJECT
private slots:
    void recognisedSkillBare() {
        QCOMPARE(skillLabel(QStringLiteral("Runecraft")), QStringLiteral("Training Runecrafting"));
    }

    void recognisedSkillWithTrainingColonPrefix() {
        QCOMPARE(skillLabel(QStringLiteral("Training: Runecraft")), QStringLiteral("Training Runecrafting"));
    }

    void caseInsensitiveMatch() {
        QCOMPARE(skillLabel(QStringLiteral("woodcutting")), QStringLiteral("Training Woodcutting"));
    }

    void nonSkillPassesThroughVerbatim() {
        QCOMPARE(skillLabel(QStringLiteral("Fighting: Zulrah")), QStringLiteral("Fighting: Zulrah"));
    }

    void emptyStaysEmpty() {
        QCOMPARE(skillLabel(QString()), QString());
    }

    // Live-verified forms from RuneLite's built-in Discord plugin `details`
    // field (see docs/superpowers/plans/2026-07-17-runelite-afk-idle-presence.md).
    void liveMiningDetailsForm() {
        QCOMPARE(skillLabel(QStringLiteral("Training: Mining")), QStringLiteral("Training Mining"));
    }

    void liveCraftingDetailsForm() {
        QCOMPARE(skillLabel(QStringLiteral("Training: Crafting")), QStringLiteral("Training Crafting"));
    }
};

QTEST_MAIN(TestSkillLabel)
#include "test_skill_label.moc"

// SkillLabel.cpp — see SkillLabel.h.
#include "SkillLabel.h"
#include <QMap>
#include <QRegularExpression>

namespace omni {

namespace {

// Display names keyed by lowercased skill token. Runecraft's plugin/in-game
// token is "Runecraft" but the display name is "Runecrafting".
const QMap<QString, QString>& skillDisplayNames() {
    static const QMap<QString, QString> kSkills = {
        {QStringLiteral("attack"),       QStringLiteral("Attack")},
        {QStringLiteral("strength"),     QStringLiteral("Strength")},
        {QStringLiteral("defence"),      QStringLiteral("Defence")},
        {QStringLiteral("ranged"),       QStringLiteral("Ranged")},
        {QStringLiteral("prayer"),       QStringLiteral("Prayer")},
        {QStringLiteral("magic"),        QStringLiteral("Magic")},
        {QStringLiteral("runecraft"),    QStringLiteral("Runecrafting")},
        {QStringLiteral("construction"), QStringLiteral("Construction")},
        {QStringLiteral("hitpoints"),    QStringLiteral("Hitpoints")},
        {QStringLiteral("agility"),      QStringLiteral("Agility")},
        {QStringLiteral("herblore"),     QStringLiteral("Herblore")},
        {QStringLiteral("thieving"),     QStringLiteral("Thieving")},
        {QStringLiteral("crafting"),     QStringLiteral("Crafting")},
        {QStringLiteral("fletching"),    QStringLiteral("Fletching")},
        {QStringLiteral("slayer"),       QStringLiteral("Slayer")},
        {QStringLiteral("hunter"),       QStringLiteral("Hunter")},
        {QStringLiteral("mining"),       QStringLiteral("Mining")},
        {QStringLiteral("smithing"),     QStringLiteral("Smithing")},
        {QStringLiteral("fishing"),      QStringLiteral("Fishing")},
        {QStringLiteral("cooking"),      QStringLiteral("Cooking")},
        {QStringLiteral("firemaking"),   QStringLiteral("Firemaking")},
        {QStringLiteral("woodcutting"),  QStringLiteral("Woodcutting")},
        {QStringLiteral("farming"),      QStringLiteral("Farming")},
    };
    return kSkills;
}

} // namespace

QString skillLabel(const QString& activity) {
    QString a = activity.trimmed();
    if (a.isEmpty()) return a;

    // Strip a leading "Training:" or "Training " (case-insensitive) before
    // matching, so both the plugin's "Training: Mining" form and a bare
    // "Mining" form resolve to the same skill key.
    static const QRegularExpression trainingPrefix(
        QStringLiteral("^training[:\\s]+"), QRegularExpression::CaseInsensitiveOption);
    QString key = a;
    key.remove(trainingPrefix);
    key = key.trimmed();

    const auto& skills = skillDisplayNames();
    const auto it = skills.constFind(key.toLower());
    if (it != skills.constEnd()) {
        return QStringLiteral("Training ") + it.value();
    }
    return a; // faithful passthrough for bosses/minigames (e.g. "Fighting: Zulrah")
}

} // namespace omni

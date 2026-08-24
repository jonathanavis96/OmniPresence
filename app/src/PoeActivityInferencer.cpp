// PoeActivityInferencer.cpp — see PoeActivityInferencer.h.
#include "PoeActivityInferencer.h"
#include <QRegularExpression>
#include <QHash>

namespace OmniPresence {

QJsonObject PoeContext::toJson() const {
    QJsonObject o;
    o[QStringLiteral("zone")]           = zone;
    o[QStringLiteral("zoneCategory")]   = zoneCategory;
    o[QStringLiteral("activity")]       = activity;
    o[QStringLiteral("character")]      = character;
    o[QStringLiteral("characterClass")] = characterClass;
    o[QStringLiteral("level")]          = level;
    o[QStringLiteral("deaths")]         = deaths;
    o[QStringLiteral("zoneEnteredAt")]  = zoneEnteredAt.isValid()
        ? zoneEnteredAt.toString(Qt::ISODate) : QString();
    o[QStringLiteral("afk")]            = afk;
    o[QStringLiteral("focused")]        = focused;
    return o;
}

void PoeActivityInferencer::setConfiguredCharacters(const QStringList& names) {
    m_configuredCharacters.clear();
    for (const QString& n : names) {
        const QString trimmed = n.trimmed();
        if (!trimmed.isEmpty()) m_configuredCharacters << trimmed.toLower();
    }
}

bool PoeActivityInferencer::isConfiguredCharacter(const QString& name) const {
    return m_configuredCharacters.contains(name.trimmed().toLower());
}

// Display verbs are this pass's own choice — the design's output table gives
// examples ("Mapping" / "In Hideout" / "Running Delve") but not an exhaustive
// mapping for every category, so the remaining entries are picked here and
// recorded in the PR rather than left unspecified.
QString PoeActivityInferencer::activityForCategory(const QString& category) {
    static const QHash<QString, QString> kLabels = {
        {QStringLiteral("hideout"),    QStringLiteral("In Hideout")},
        {QStringLiteral("town"),       QStringLiteral("In Town")},
        {QStringLiteral("map"),        QStringLiteral("Mapping")},
        {QStringLiteral("campaign"),   QStringLiteral("Adventuring")},
        {QStringLiteral("delve"),      QStringLiteral("Running Delve")},
        {QStringLiteral("labyrinth"),  QStringLiteral("Running the Labyrinth")},
        {QStringLiteral("sanctum"),    QStringLiteral("Running the Sanctum")},
        {QStringLiteral("heist"),      QStringLiteral("On a Heist")},
        {QStringLiteral("menagerie"),  QStringLiteral("In the Menagerie")},
        {QStringLiteral("boss"),       QStringLiteral("Boss Fight")},
        {QStringLiteral("memory"),     QStringLiteral("Exploring a Memory")},
        {QStringLiteral("simulacrum"), QStringLiteral("Fighting the Simulacrum")},
        {QStringLiteral("other"),      QStringLiteral("Exploring")},
    };
    return kLabels.value(category, QStringLiteral("Exploring"));
}

void PoeActivityInferencer::resetSession() {
    // Session stats (deaths, zone) reset on LOG FILE OPENING per the design —
    // but the configured-character list is a standing config setting, not
    // session state, so it survives the reset.
    const QStringList configured = m_configuredCharacters;
    m_context = PoeContext{};
    m_context.sessionActive = true;
    m_configuredCharacters  = configured;
}

void PoeActivityInferencer::onZoneEntered(const QString& zone) {
    m_context.zone         = zone;
    m_context.zoneCategory = m_zoneTable ? m_zoneTable->classify(zone) : QStringLiteral("unknown");
    m_context.activity     = activityForCategory(m_context.zoneCategory);
    m_context.zoneEnteredAt = QDateTime::currentDateTimeUtc();
    m_context.sessionActive = true;
}

void PoeActivityInferencer::onLevelUp(const QString& character, const QString& klass, int level) {
    if (!isConfiguredCharacter(character)) return;   // not our character — ignore per design
    m_context.character      = character;
    m_context.characterClass = klass;
    m_context.level          = level;
}

void PoeActivityInferencer::onDeath(const QString& character) {
    if (!isConfiguredCharacter(character)) return;   // not our character — ignore per design
    m_context.deaths += 1;
}

void PoeActivityInferencer::onAfk(bool on)      { m_context.afk     = on;     }
void PoeActivityInferencer::onFocus(bool gained){ m_context.focused = gained; }

void PoeActivityInferencer::processLine(const QString& line) {
    // Privacy boundary: the log carries whispers and public chat verbatim
    // ("@From <name> Sender: text" / "@To Receiver: text"). Bail before any
    // pattern below gets a chance to false-positive on player-controlled
    // text — chat is never parsed, stored, forwarded, or logged.
    if (line.contains(QLatin1String("@From ")) || line.contains(QLatin1String("@To "))) {
        return;
    }

    if (line.contains(QLatin1String("LOG FILE OPENING"))) {
        resetSession();
        return;
    }

    static const QRegularExpression kZone(
        QStringLiteral(": You have entered (.+)\\.\\s*$"));
    static const QRegularExpression kLevel(
        QStringLiteral(": (.+?) \\((.+?)\\) is now level (\\d+)"));
    static const QRegularExpression kDeath(
        QStringLiteral(": (.+?) has been slain\\.\\s*$"));
    static const QRegularExpression kAfk(
        QStringLiteral(": AFK mode is now (ON|OFF)\\.\\s*$"));

    QRegularExpressionMatch m;

    if ((m = kZone.match(line)).hasMatch()) {
        onZoneEntered(m.captured(1).trimmed());
        return;
    }
    if ((m = kLevel.match(line)).hasMatch()) {
        onLevelUp(m.captured(1).trimmed(), m.captured(2).trimmed(), m.captured(3).toInt());
        return;
    }
    if ((m = kDeath.match(line)).hasMatch()) {
        onDeath(m.captured(1).trimmed());
        return;
    }
    if ((m = kAfk.match(line)).hasMatch()) {
        onAfk(m.captured(1) == QLatin1String("ON"));
        return;
    }
    if (line.contains(QLatin1String("[WINDOW] Gained focus"))) {
        onFocus(true);
        return;
    }
    if (line.contains(QLatin1String("[WINDOW] Lost focus"))) {
        onFocus(false);
        return;
    }
    // "[SCENE] Set Source [...]" (duplicate zone signal, cross-check only),
    // "[LOADING SCREEN] (...) Duration = ..." and "Connecting to instance
    // server at ..." are informational per the design and intentionally not
    // parsed further — nothing downstream needs them yet, and matching them
    // ties the parser to details more likely to shift between log versions
    // than the plain-English lines above.
}

} // namespace OmniPresence

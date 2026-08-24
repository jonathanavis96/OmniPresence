// PoeActivityInferencer.h — Pure parsing/inference over Path of Exile's
// Client.txt / LatestClient.txt lines. No I/O; PoeLogWatcher feeds it
// complete lines. Mirrors the RuneLite plugin's ActivityInferencer.
//
// This is the privacy boundary described in the design doc: any line that
// does not match the allowlisted patterns below (zone/level/death/focus/
// AFK/session-start) is discarded immediately. Chat/whisper lines are never
// parsed, stored, forwarded, or logged.
#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include "PoeZoneTable.h"

namespace OmniPresence {

/// Snapshot of everything currently known about the player's Path of Exile
/// session, derived purely from parsed log lines.
struct PoeContext {
    QString   zone;
    QString   zoneCategory{QStringLiteral("unknown")};
    QString   activity;              ///< Display verb derived from zoneCategory, e.g. "Mapping".
    QString   character;             ///< Empty unless a line's name matched a configured character.
    QString   characterClass;
    int       level{0};
    int       deaths{0};
    QDateTime zoneEnteredAt;
    bool      afk{false};
    bool      focused{false};
    bool      sessionActive{false};  ///< False until the first zone/session-start line is seen.

    [[nodiscard]] QJsonObject toJson() const;
};

class PoeActivityInferencer {
public:
    /// Character names (case-insensitive) whose level/class/death lines are
    /// allowed to update session state. Empty = degrade to zone-only presence
    /// — the design explicitly rejects a "most frequent name this session"
    /// heuristic, since it silently publishes a stranger's name as yours.
    void setConfiguredCharacters(const QStringList& names);

    /// Zone name -> category classifier. Left null, everything classifies
    /// "unknown" rather than crashing.
    void setZoneTable(const PoeZoneTable* table) { m_zoneTable = table; }

    /// Feed one complete log line (CR/LF already stripped). Updates internal
    /// session state; lines outside the allowlist are discarded with no effect.
    void processLine(const QString& line);

    [[nodiscard]] const PoeContext& context() const noexcept { return m_context; }

private:
    void resetSession();
    void onZoneEntered(const QString& zone);
    void onLevelUp(const QString& character, const QString& klass, int level);
    void onDeath(const QString& character);
    void onAfk(bool on);
    void onFocus(bool gained);

    [[nodiscard]] bool isConfiguredCharacter(const QString& name) const;
    [[nodiscard]] static QString activityForCategory(const QString& category);

    QStringList         m_configuredCharacters;   ///< Lowercased.
    const PoeZoneTable* m_zoneTable{nullptr};
    PoeContext          m_context;
};

} // namespace OmniPresence

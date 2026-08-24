// PoeZoneTable.h — Path of Exile zone-name -> category classifier.
//
// Loads config/poe-zones.json (ordered regex patterns, then exact lookups,
// then "unknown") built and independently verified against a live 352-zone
// player log — see docs/superpowers/specs/2026-08-11-path-of-exile-integration-design.md.
// Data, not code: GGG adds zones every league, so this ships as JSON rather
// than a hardcoded switch that goes stale.
#pragma once

#include <QString>
#include <QJsonObject>
#include <QList>
#include <QHash>
#include <QSet>
#include <QRegularExpression>

namespace OmniPresence {

/// Pure lookup table, no I/O beyond the explicit load call — fully
/// unit-testable like PoeActivityInferencer.
class PoeZoneTable {
public:
    /// Parse patterns/exact/ambiguous from an already-loaded JSON object.
    /// Malformed or missing sections are treated as empty rather than
    /// erroring, so a corrupt file degrades every zone to "unknown" instead
    /// of crashing.
    void loadFromJson(const QJsonObject& obj);

    /// Read and parse a zone table file from disk. Returns false (leaving any
    /// previously loaded table untouched) if the file is missing or invalid.
    bool loadFromFile(const QString& path);

    /// Classify a zone name: ordered pattern match first (first match wins),
    /// then exact lookup, then "unknown".
    [[nodiscard]] QString classify(const QString& zoneName) const;

    /// True for a zone name PoE reuses across both a campaign zone and an
    /// Atlas map (e.g. "Dunes") — informational only; classify() already
    /// resolves these to their listed (more-likely-endgame) category.
    [[nodiscard]] bool isAmbiguous(const QString& zoneName) const;

    [[nodiscard]] int patternCount() const noexcept { return m_patterns.size(); }
    [[nodiscard]] int exactCount()   const noexcept { return m_exact.size(); }

private:
    struct PatternEntry {
        QRegularExpression regex;
        QString            category;
    };
    QList<PatternEntry>     m_patterns;
    QHash<QString, QString> m_exact;
    QSet<QString>           m_ambiguous;
};

} // namespace OmniPresence

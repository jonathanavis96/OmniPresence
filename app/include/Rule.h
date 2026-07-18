// Rule.h — Data model for a single presence rule and an ordered rule set.
#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QList>

namespace OmniPresence {

// ── Enumerations ──────────────────────────────────────────────────────────────

enum class ActivityType {
    Playing    = 0,
    Listening  = 1,
    Watching   = 2,
    Competing  = 3,
    Custom     = 4,
};

/// Canonical ActivityType <-> config-string mapping (shared by Rule and the
/// custom-override config so both serialise the type identically).
[[nodiscard]] QString      activityTypeToString(ActivityType t);
[[nodiscard]] ActivityType activityTypeFromString(const QString& s);

enum class TimestampMode {
    None            = 0,  ///< No timestamp shown.
    StartNow        = 1,  ///< Set timestamp when this rule first matches.
    Keep            = 2,  ///< Keep the timestamp from the previous presence.
    CategoryChange  = 3,  ///< Reset timestamp only when the activity category changes.
};

enum class PrivacyLevel {
    Public      = 0,  ///< Show full details.
    DomainOnly  = 1,  ///< For browser rules — show domain but not page title.
    Private     = 2,  ///< Suppress all details; use fallback.
};

// ── Rule ─────────────────────────────────────────────────────────────────────

/// A single ordered rule.  Rules are matched in descending priority order.
struct Rule {
    // Metadata
    QString id;                   ///< UUID-style stable identifier.
    QString name;                 ///< Human-readable label shown in the UI.
    bool    enabled{true};
    int     priority{100};        ///< Higher = evaluated first.

    // Match criteria (all non-empty criteria must match; empty = wildcard)
    QString matchProcessName;         ///< Substring match against WindowInfo::processName.
    QString matchExecutablePath;      ///< Substring match against executablePath.
    QString matchWindowTitle;         ///< Substring or regex match against windowTitle.
    bool    matchWindowTitleRegex{false};
    QString matchBrowserDomain;       ///< From IntegrationContext browser payload.
    QString matchBrowserCategory;     ///< From IntegrationContext browser payload.
    QString matchIntegrationSource;   ///< e.g. "runelite", "vscode", "terminal".

    // Output templates (support {{variable}} syntax)
    ActivityType activityType{ActivityType::Playing};
    QString      activityNameTemplate;     ///< Discord "name" field (broad label).
    QString      detailsTemplate;          ///< Discord "details" field (what I'm doing).
    QString      stateTemplate;            ///< Discord "state" field (specific context).
    QString      largeImageKey;
    QString      largeImageText;
    QString      smallImageKey;
    QString      smallImageText;

    TimestampMode timestampMode{TimestampMode::CategoryChange};
    PrivacyLevel  privacyLevel{PrivacyLevel::Public};

    // ── Serialisation ─────────────────────────────────────────────────────────
    [[nodiscard]] QJsonObject toJson()    const;
    static Rule               fromJson(const QJsonObject& obj);

    /// Returns true if this rule matches the given process/title/integration combo.
    /// Actual template resolution is done by RuleEngine after matching.
    [[nodiscard]] bool matches(const QString& processName,
                                const QString& executablePath,
                                const QString& windowTitle,
                                const QString& browserDomain,
                                const QString& browserCategory,
                                const QString& integrationSource) const;
};

// ── RuleSet ───────────────────────────────────────────────────────────────────

/// An ordered, priority-sorted list of rules.
class RuleSet {
public:
    void addRule(const Rule& rule);
    void removeRule(const QString& id);
    void updateRule(const Rule& rule);

    /// Returns rules sorted by priority (highest first).
    [[nodiscard]] QList<Rule> sortedRules() const;
    [[nodiscard]] const QList<Rule>& rules() const noexcept { return m_rules; }

    [[nodiscard]] QJsonObject toJson()     const;
    static RuleSet            fromJson(const QJsonObject& obj);

private:
    QList<Rule> m_rules;
};

} // namespace OmniPresence

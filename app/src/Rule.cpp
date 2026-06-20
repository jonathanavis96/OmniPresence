// Rule.cpp — JSON serialisation and matching logic for Rule / RuleSet.
#include "Rule.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

namespace OmniPresence {

// ── Helper: enum <-> string ───────────────────────────────────────────────────

static QString activityTypeToString(ActivityType t) {
    switch (t) {
        case ActivityType::Playing:   return QStringLiteral("Playing");
        case ActivityType::Listening: return QStringLiteral("Listening");
        case ActivityType::Watching:  return QStringLiteral("Watching");
        case ActivityType::Competing: return QStringLiteral("Competing");
        case ActivityType::Custom:    return QStringLiteral("Custom");
    }
    return QStringLiteral("Playing");
}

static ActivityType activityTypeFromString(const QString& s) {
    if (s == QLatin1String("Listening"))  return ActivityType::Listening;
    if (s == QLatin1String("Watching"))   return ActivityType::Watching;
    if (s == QLatin1String("Competing"))  return ActivityType::Competing;
    if (s == QLatin1String("Custom"))     return ActivityType::Custom;
    return ActivityType::Playing;
}

static QString timestampModeToString(TimestampMode m) {
    switch (m) {
        case TimestampMode::None:           return QStringLiteral("None");
        case TimestampMode::StartNow:       return QStringLiteral("StartNow");
        case TimestampMode::Keep:           return QStringLiteral("Keep");
        case TimestampMode::CategoryChange: return QStringLiteral("CategoryChange");
    }
    return QStringLiteral("CategoryChange");
}

static TimestampMode timestampModeFromString(const QString& s) {
    if (s == QLatin1String("None"))     return TimestampMode::None;
    if (s == QLatin1String("StartNow")) return TimestampMode::StartNow;
    if (s == QLatin1String("Keep"))     return TimestampMode::Keep;
    return TimestampMode::CategoryChange;
}

static QString privacyLevelToString(PrivacyLevel p) {
    switch (p) {
        case PrivacyLevel::Public:     return QStringLiteral("Public");
        case PrivacyLevel::DomainOnly: return QStringLiteral("DomainOnly");
        case PrivacyLevel::Private:    return QStringLiteral("Private");
    }
    return QStringLiteral("Public");
}

static PrivacyLevel privacyLevelFromString(const QString& s) {
    if (s == QLatin1String("DomainOnly")) return PrivacyLevel::DomainOnly;
    if (s == QLatin1String("Private"))    return PrivacyLevel::Private;
    return PrivacyLevel::Public;
}

// ── Rule::toJson / fromJson ───────────────────────────────────────────────────

QJsonObject Rule::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("id")]                    = id;
    obj[QStringLiteral("name")]                  = name;
    obj[QStringLiteral("enabled")]               = enabled;
    obj[QStringLiteral("priority")]              = priority;
    obj[QStringLiteral("matchProcessName")]      = matchProcessName;
    obj[QStringLiteral("matchExecutablePath")]   = matchExecutablePath;
    obj[QStringLiteral("matchWindowTitle")]      = matchWindowTitle;
    obj[QStringLiteral("matchWindowTitleRegex")] = matchWindowTitleRegex;
    obj[QStringLiteral("matchBrowserDomain")]    = matchBrowserDomain;
    obj[QStringLiteral("matchBrowserCategory")]  = matchBrowserCategory;
    obj[QStringLiteral("matchIntegrationSource")]= matchIntegrationSource;
    obj[QStringLiteral("activityType")]          = activityTypeToString(activityType);
    obj[QStringLiteral("activityNameTemplate")]  = activityNameTemplate;
    obj[QStringLiteral("detailsTemplate")]       = detailsTemplate;
    obj[QStringLiteral("stateTemplate")]         = stateTemplate;
    obj[QStringLiteral("largeImageKey")]         = largeImageKey;
    obj[QStringLiteral("largeImageText")]        = largeImageText;
    obj[QStringLiteral("smallImageKey")]         = smallImageKey;
    obj[QStringLiteral("smallImageText")]        = smallImageText;
    obj[QStringLiteral("timestampMode")]         = timestampModeToString(timestampMode);
    obj[QStringLiteral("privacyLevel")]          = privacyLevelToString(privacyLevel);
    return obj;
}

Rule Rule::fromJson(const QJsonObject& obj) {
    Rule r;
    r.id                    = obj[QStringLiteral("id")].toString();
    r.name                  = obj[QStringLiteral("name")].toString();
    r.enabled               = obj[QStringLiteral("enabled")].toBool(true);
    r.priority              = obj[QStringLiteral("priority")].toInt(100);
    r.matchProcessName      = obj[QStringLiteral("matchProcessName")].toString();
    r.matchExecutablePath   = obj[QStringLiteral("matchExecutablePath")].toString();
    r.matchWindowTitle      = obj[QStringLiteral("matchWindowTitle")].toString();
    r.matchWindowTitleRegex = obj[QStringLiteral("matchWindowTitleRegex")].toBool(false);
    r.matchBrowserDomain    = obj[QStringLiteral("matchBrowserDomain")].toString();
    r.matchBrowserCategory  = obj[QStringLiteral("matchBrowserCategory")].toString();
    r.matchIntegrationSource= obj[QStringLiteral("matchIntegrationSource")].toString();
    r.activityType          = activityTypeFromString(obj[QStringLiteral("activityType")].toString());
    r.activityNameTemplate  = obj[QStringLiteral("activityNameTemplate")].toString();
    r.detailsTemplate       = obj[QStringLiteral("detailsTemplate")].toString();
    r.stateTemplate         = obj[QStringLiteral("stateTemplate")].toString();
    r.largeImageKey         = obj[QStringLiteral("largeImageKey")].toString();
    r.largeImageText        = obj[QStringLiteral("largeImageText")].toString();
    r.smallImageKey         = obj[QStringLiteral("smallImageKey")].toString();
    r.smallImageText        = obj[QStringLiteral("smallImageText")].toString();
    r.timestampMode         = timestampModeFromString(obj[QStringLiteral("timestampMode")].toString());
    r.privacyLevel          = privacyLevelFromString(obj[QStringLiteral("privacyLevel")].toString());
    return r;
}

// ── Rule::matches ─────────────────────────────────────────────────────────────

bool Rule::matches(const QString& procName,
                   const QString& exePath,
                   const QString& winTitle,
                   const QString& browserDomain,
                   const QString& browserCategory,
                   const QString& integrationSource) const
{
    if (!enabled) return false;

    auto substringMatch = [](const QString& pattern, const QString& value) -> bool {
        return pattern.isEmpty()
            || value.contains(pattern, Qt::CaseInsensitive);
    };

    if (!substringMatch(matchProcessName,    procName))      return false;
    if (!substringMatch(matchExecutablePath, exePath))       return false;
    if (!substringMatch(matchBrowserDomain,  browserDomain)) return false;
    if (!substringMatch(matchBrowserCategory,browserCategory))return false;
    if (!substringMatch(matchIntegrationSource, integrationSource)) return false;

    // Window title: substring or regex
    if (!matchWindowTitle.isEmpty()) {
        if (matchWindowTitleRegex) {
            const QRegularExpression re(matchWindowTitle,
                QRegularExpression::CaseInsensitiveOption);
            if (!re.match(winTitle).hasMatch()) return false;
        } else {
            if (!winTitle.contains(matchWindowTitle, Qt::CaseInsensitive)) return false;
        }
    }

    return true;
}

// ── RuleSet ───────────────────────────────────────────────────────────────────

void RuleSet::addRule(const Rule& rule) {
    m_rules.append(rule);
}

void RuleSet::removeRule(const QString& id) {
    m_rules.removeIf([&](const Rule& r) { return r.id == id; });
}

void RuleSet::updateRule(const Rule& rule) {
    for (auto& r : m_rules) {
        if (r.id == rule.id) { r = rule; return; }
    }
    m_rules.append(rule);
}

QList<Rule> RuleSet::sortedRules() const {
    QList<Rule> sorted = m_rules;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Rule& a, const Rule& b) {
        return a.priority > b.priority;
    });
    return sorted;
}

QJsonObject RuleSet::toJson() const {
    QJsonArray arr;
    for (const Rule& r : m_rules) arr.append(r.toJson());
    QJsonObject obj;
    obj[QStringLiteral("rules")] = arr;
    return obj;
}

RuleSet RuleSet::fromJson(const QJsonObject& obj) {
    RuleSet rs;
    const QJsonArray arr = obj[QStringLiteral("rules")].toArray();
    for (const auto& v : arr) {
        rs.addRule(Rule::fromJson(v.toObject()));
    }
    return rs;
}

} // namespace OmniPresence

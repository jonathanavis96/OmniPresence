// RuleEngine.cpp — Deterministic priority-based presence evaluation.
//
// Priority order (spec):
//  1. Manual pause / private override  → private fallback
//  2. Manual pinned presence           → pinnedPresence
//  3. Deep integration context match   → rule with matchIntegrationSource set
//  4. Specific user rule               → any enabled rule
//  5. Browser sanitised domain rule    → rule with matchBrowserDomain/Category
//  6. Generic process rule             → rule matching only processName
//  7. Private fallback
#include "RuleEngine.h"
#include "TemplateEngine.h"
#include <QHash>
#include <QDateTime>
#include <QDebug>

namespace OmniPresence {

// ── Private fallback ──────────────────────────────────────────────────────────

PresencePayload RuleEngine::privateFallback() {
    PresencePayload p;
    p.name              = QStringLiteral("Computer");
    p.details           = QStringLiteral("Working privately");
    p.state             = QStringLiteral("Private");
    // Must be Playing — Discord Rich Presence rejects the Custom/CustomStatus
    // activity type ("Field type: Invalid enum value"); custom status is a
    // separate Discord feature that UpdateRichPresence cannot set.
    p.activityType      = ActivityType::Playing;
    p.privacyLevel      = PrivacyLevel::Private;
    p.isPrivateFallback = true;
    p.matchedRuleName   = QStringLiteral("(private fallback)");
    return p;
}

PresencePayload RuleEngine::genericPresence(const WindowInfo& window) {
    // No window at all → fall back to the private placeholder.
    if (window.processName.isEmpty()) {
        return privateFallback();
    }

    // Friendly app name from the process name (e.g. "msedge.exe" -> "Microsoft
    // Edge"). We deliberately use ONLY the app name, never window.windowTitle,
    // so titles stay private by default per the privacy design.
    QString proc = window.processName;
    if (proc.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        proc.chop(4);
    }
    static const QHash<QString, QString> kFriendly = {
        {QStringLiteral("chrome"),          QStringLiteral("Google Chrome")},
        {QStringLiteral("msedge"),          QStringLiteral("Microsoft Edge")},
        {QStringLiteral("firefox"),         QStringLiteral("Firefox")},
        {QStringLiteral("brave"),           QStringLiteral("Brave")},
        {QStringLiteral("code"),            QStringLiteral("VS Code")},
        {QStringLiteral("windowsterminal"), QStringLiteral("Windows Terminal")},
        {QStringLiteral("explorer"),        QStringLiteral("File Explorer")},
        {QStringLiteral("discord"),         QStringLiteral("Discord")},
    };
    const QString key  = proc.toLower();
    QString friendly   = kFriendly.value(key);
    if (friendly.isEmpty()) {
        // Title-case the bare process name as a reasonable default.
        friendly = proc;
        if (!friendly.isEmpty()) {
            friendly[0] = friendly[0].toUpper();
        }
    }

    PresencePayload p;
    p.name              = friendly;
    p.details           = QStringLiteral("Active");
    p.activityType      = ActivityType::Playing;
    p.privacyLevel      = PrivacyLevel::Public;
    p.isPrivateFallback = false;
    p.matchedRuleName   = QStringLiteral("(no rule — generic app)");
    return p;
}

// ── evaluate ──────────────────────────────────────────────────────────────────

PresencePayload RuleEngine::evaluate(const WindowInfo&          window,
                                     const IntegrationContext&  integrations,
                                     const RuleSet&             rules,
                                     const ManualOverrideState& overrideState,
                                     const PresencePayload&     previousPayload) const
{
    // Priority 1 — manual pause or global private mode.
    if (overrideState.paused || overrideState.privateMode) {
        return privateFallback();
    }

    // Priority 2 — manually pinned presence.
    if (overrideState.pinnedPresence.has_value()) {
        return overrideState.pinnedPresence.value();
    }

    const QList<Rule> sorted = rules.sortedRules();

    // Priority 3 — deep integration context match (rule requires integration data).
    {
        const auto opt = matchRule(sorted, window, integrations,
                                   /*requireIntegration=*/true,
                                   /*genericProcessOnly=*/false);
        if (opt.has_value()) {
            return resolveRule(*opt, window, integrations, previousPayload);
        }
    }

    // Priority 4 — specific user rule (any criteria, not just generic process).
    // Priority 5 — browser domain/category rule — these are just regular rules
    //              where matchBrowserDomain/Category is set; they are naturally
    //              less specific than rules with more criteria, so we rely on
    //              the user assigning lower priority values to them.
    {
        const auto opt = matchRule(sorted, window, integrations,
                                   /*requireIntegration=*/false,
                                   /*genericProcessOnly=*/false);
        if (opt.has_value()) {
            return resolveRule(*opt, window, integrations, previousPayload);
        }
    }

    // Priority 6 — generic process rule (would have been caught above already;
    //              this branch is intentionally left here for documentary clarity
    //              and in case the rule-matching strategy is tightened later).

    // Priority 7 — no rule matched. Private mode is already handled at priority 1,
    // so reaching here means private mode is OFF: show a generic, title-safe
    // presence for the active app rather than the private "Computer" fallback.
    return genericPresence(window);
}

// ── matchRule ─────────────────────────────────────────────────────────────────

std::optional<Rule> RuleEngine::matchRule(const QList<Rule>&        sortedRules,
                                          const WindowInfo&          window,
                                          const IntegrationContext&  integrations,
                                          bool                       requireIntegration,
                                          bool                       genericProcessOnly) const
{
    const QString browserDomain   = integrations.browserDomain();
    const QString browserCategory = integrations.browserCategory();

    // Determine which integration source is "active" for the current app.
    // Heuristic: browser is active when chrome/firefox/edge has focus;
    // runelite/vscode/terminal are identified by their process names.
    QString activeIntegrationSource;
    {
        const QString pn = window.processName.toLower();
        if (pn.contains(QLatin1String("chrome")) ||
            pn.contains(QLatin1String("firefox")) ||
            pn.contains(QLatin1String("msedge")) ||
            pn.contains(QLatin1String("brave"))) {
            activeIntegrationSource = QStringLiteral("browser");
        } else if (pn.contains(QLatin1String("runelite"))) {
            activeIntegrationSource = QStringLiteral("runelite");
        } else if (pn == QLatin1String("code.exe")) {
            activeIntegrationSource = QStringLiteral("vscode");
        } else if (pn.contains(QLatin1String("terminal")) ||
                   pn.contains(QLatin1String("wt.exe"))   ||
                   pn.contains(QLatin1String("cmd.exe"))  ||
                   pn.contains(QLatin1String("powershell"))) {
            activeIntegrationSource = QStringLiteral("terminal");
        }
    }

    for (const Rule& rule : sortedRules) {
        if (!rule.enabled) continue;

        // If we are in the "requireIntegration" pass, skip rules that don't
        // have an integration source, or whose source doesn't have fresh data.
        if (requireIntegration) {
            if (rule.matchIntegrationSource.isEmpty()) continue;
            if (integrations.getFresh(rule.matchIntegrationSource) == nullptr) continue;
        }

        if (!rule.matches(window.processName,
                          window.executablePath,
                          window.windowTitle,
                          browserDomain,
                          browserCategory,
                          activeIntegrationSource)) {
            continue;
        }
        return rule;
    }
    return std::nullopt;
}

// ── resolveRule ───────────────────────────────────────────────────────────────

PresencePayload RuleEngine::resolveRule(const Rule&               rule,
                                        const WindowInfo&          window,
                                        const IntegrationContext&  integrations,
                                        const PresencePayload&     previousPayload) const
{
    const TemplateContext ctx = TemplateEngine::buildContext(window, integrations);

    PresencePayload p;
    p.activityType   = rule.activityType;
    p.name           = TemplateEngine::render(rule.activityNameTemplate, ctx);
    p.details        = TemplateEngine::render(rule.detailsTemplate,      ctx);
    p.state          = TemplateEngine::render(rule.stateTemplate,        ctx);
    p.largeImageKey  = rule.largeImageKey;
    p.largeImageText = rule.largeImageText;
    p.smallImageKey  = rule.smallImageKey;
    p.smallImageText = rule.smallImageText;
    p.privacyLevel   = rule.privacyLevel;
    p.matchedRuleName= rule.name;
    p.timestampMode  = rule.timestampMode;

    // Privacy override — if the rule itself is private, blank out details/state.
    if (rule.privacyLevel == PrivacyLevel::Private) {
        return privateFallback();
    }
    if (rule.privacyLevel == PrivacyLevel::DomainOnly) {
        p.details = integrations.browserDomain();
        p.state   = {};
    }

    // The activity name IS the main line the user controls (e.g.
    // "RuneLight – Training Crafting"), so the compact member-list status always
    // shows Name. Details/state fill the expanded profile card + side panel.
    p.statusDisplay = StatusDisplay::Name;

    // Timestamp resolution.
    switch (rule.timestampMode) {
        case TimestampMode::None:
            p.activityStartedAt = {};
            break;
        case TimestampMode::StartNow:
            p.activityStartedAt = QDateTime::currentDateTimeUtc();
            break;
        case TimestampMode::Keep:
            p.activityStartedAt = previousPayload.activityStartedAt;
            break;
        case TimestampMode::CategoryChange:
            // Reset only when the process category has changed.
            if (previousPayload.matchedRuleName != rule.name ||
                previousPayload.activityStartedAt.isNull()) {
                p.activityStartedAt = QDateTime::currentDateTimeUtc();
            } else {
                p.activityStartedAt = previousPayload.activityStartedAt;
            }
            break;
    }

    return p;
}

} // namespace OmniPresence

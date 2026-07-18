// RuleEngine.cpp — Deterministic priority-based presence evaluation.
//
// Priority order (spec):
//  0. Input-idle override (AFK / Away) → awayPresence / afkPresence
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
#include <QRegularExpression>
#include <QDebug>

namespace OmniPresence {

namespace {

// Reused as the AFK-tier icon: prefer whatever largeImageKey the user's own
// RuneLite rule is configured with (single source of truth — follows the
// user if they change the icon), falling back to the shipped default URL
// only if no RuneLite rule exists at all.
QString runeliteIconKey(const RuleSet& rules) {
    for (const Rule& r : rules.rules()) {
        if (r.matchProcessName.compare(QStringLiteral("RuneLite.exe"), Qt::CaseInsensitive) == 0 ||
            r.matchIntegrationSource == QLatin1String("runelite")) {
            if (!r.largeImageKey.isEmpty()) return r.largeImageKey;
        }
    }
    return QStringLiteral(
        "https://raw.githubusercontent.com/jonathanavis96/OmniPresence/omnipresence-work/assets/icons/osrs.png");
}

} // namespace

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

// ── Idle-tier payloads ──────────────────────────────────────────────────────────

PresencePayload RuleEngine::awayPresence(const IdleConfig& idle) {
    // Any app, idle >= awaySeconds — deliberately drops app identity entirely
    // (no largeImageKey/name from the focused window survives here).
    PresencePayload p;
    p.name              = idle.awayLabel;
    p.details           = idle.awayLabel;
    p.state             = QString();
    p.activityType      = ActivityType::Playing;
    p.privacyLevel      = PrivacyLevel::Public;
    p.isPrivateFallback = false;
    p.statusDisplay     = StatusDisplay::Name;
    p.largeImageKey     = idle.awayImageKey;
    p.matchedRuleName   = QStringLiteral("(idle — away)");
    return p;
}

PresencePayload RuleEngine::afkPresence(const IdleConfig& idle, const QString& largeImageKey) {
    // RuneLite focused, idle >= afkSeconds — keeps the OSRS icon/name, but the
    // details line is replaced with the AFK label (skill/location are hidden).
    PresencePayload p;
    p.name              = QStringLiteral("OSRS");
    p.details           = idle.afkLabel;
    p.state             = QString();
    p.activityType      = ActivityType::Playing;
    p.privacyLevel      = PrivacyLevel::Public;
    p.isPrivateFallback = false;
    p.statusDisplay     = StatusDisplay::Name;
    p.largeImageKey     = largeImageKey;
    p.matchedRuleName   = QStringLiteral("(idle — AFK)");
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
                                     const PresencePayload&     previousPayload,
                                     quint64                    idleSeconds,
                                     const QString&              focusedProcessName,
                                     const IdleConfig&           idle) const
{
    // Priority 0 — input-idle override (AFK / Away-from-computer). Fires ahead of
    // pinned presence and normal rules so presence reflects "nobody is at the
    // keyboard" the instant the threshold is crossed. It must NOT override the
    // user's privacy controls, though: when updates are paused or private mode is
    // on we skip idle entirely and fall through to the private fallback below —
    // broadcasting a public "Away" / "AFK" card while the user asked to pause or
    // stay private would be a privacy leak. Higher threshold (awaySeconds) is
    // checked FIRST so Away always wins over AFK when both are satisfied
    // simultaneously (e.g. idle 20 min: away, not AFK).
    if (idle.enabled && !overrideState.paused && !overrideState.privateMode) {
        if (idleSeconds >= idle.awaySeconds) {
            return awayPresence(idle);
        }
        if (idleSeconds >= idle.afkSeconds &&
            focusedProcessName.compare(QStringLiteral("RuneLite.exe"), Qt::CaseInsensitive) == 0) {
            return afkPresence(idle, runeliteIconKey(rules));
        }
    }

    // Priority 1 — manual pause or global private mode.
    if (overrideState.paused || overrideState.privateMode) {
        return privateFallback();
    }

    // Priority 2 — manually pinned presence.
    if (overrideState.pinnedPresence.has_value()) {
        return overrideState.pinnedPresence.value();
    }

    const QList<Rule> sorted = rules.sortedRules();

    // A matched rule that renders a BLANK name must never win: Discord shows an
    // empty activity name as the bare application name ("OmniPresence"). This
    // happens e.g. when the RuneLite window is focused but its integration feed
    // has gone stale, so {{runelite.activity or runelite.location}} resolves to
    // "". In that case we fall through to the next priority (ultimately
    // genericPresence) rather than publish an empty name.
    auto resolveIfNamed = [&](const Rule& r) -> std::optional<PresencePayload> {
        PresencePayload p = resolveRule(r, window, integrations, previousPayload);
        if (p.name.isEmpty()) return std::nullopt;
        return p;
    };

    // Priority 3 — deep integration context match (rule requires integration data).
    {
        const auto opt = matchRule(sorted, window, integrations,
                                   /*requireIntegration=*/true,
                                   /*genericProcessOnly=*/false);
        if (opt.has_value()) {
            if (auto p = resolveIfNamed(*opt)) return *p;
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
            if (auto p = resolveIfNamed(*opt)) return *p;
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

    // Determine which integration source is "active" for the FOCUSED window.
    // Presence follows focus, so an integration rule only wins while its own app
    // is in the foreground. Detection is by process name, except the RuneLite
    // *dev* client (a locally-built source jar — the only way to sideload our
    // plugin) runs as java.exe, so we additionally recognise it by window title.
    QString activeIntegrationSource;
    {
        const QString pn = window.processName.toLower();
        const QString wt = window.windowTitle.toLower();
        if (pn.contains(QLatin1String("chrome")) ||
            pn.contains(QLatin1String("firefox")) ||
            pn.contains(QLatin1String("msedge")) ||
            pn.contains(QLatin1String("brave"))) {
            activeIntegrationSource = QStringLiteral("browser");
        } else if (pn.contains(QLatin1String("runelite")) ||
                   ((pn.contains(QLatin1String("java")) ||
                     pn.contains(QLatin1String("javaw"))) &&
                    (wt.contains(QLatin1String("runelite")) ||
                     wt.contains(QLatin1String("old school"))))) {
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

        // ── Integration pass (priority 3) ─────────────────────────────────────
        // An integration rule wins only when (a) its source has fresh data AND
        // (b) its own app is the FOCUSED window. The focus gate is what lets
        // presence follow you between apps — swapping to Claude/YouTube must not
        // stay stuck on a still-fresh RuneLite feed.
        if (requireIntegration) {
            if (rule.matchIntegrationSource.isEmpty()) continue;
            if (rule.matchIntegrationSource != activeIntegrationSource) continue;
            if (integrations.getFresh(rule.matchIntegrationSource) == nullptr) continue;

            // Browser: several rules share source "browser", so the domain must
            // still discriminate which one wins.
            if (rule.matchIntegrationSource == QLatin1String("browser")) {
                if (!rule.matches(window.processName, window.executablePath,
                                  window.windowTitle, browserDomain,
                                  browserCategory, activeIntegrationSource)) {
                    continue;
                }
                qDebug() << "[RuleEngine] integration(browser) rule matched:"
                         << rule.name << "domain=" << browserDomain;
                return rule;
            }

            // RuneLite is the one narrow exception: its dev client runs as
            // java.exe, so a "RuneLite.exe" process criterion would wrongly
            // reject it. There we accept on focus + fresh feed alone.
            //
            // Every other source (terminal/vscode) must still honour its own
            // process/title/path criteria — otherwise the Windows Terminal rule
            // (matchProcessName "WindowsTerminal.exe") would wrongly apply to
            // cmd.exe/PowerShell whenever terminal context is fresh, and multiple
            // rules sharing a source could not discriminate between apps.
            if (rule.matchIntegrationSource != QLatin1String("runelite")) {
                if (!rule.matches(window.processName, window.executablePath,
                                  window.windowTitle, browserDomain,
                                  browserCategory, activeIntegrationSource)) {
                    continue;
                }
            }
            qDebug() << "[RuleEngine] integration(" << rule.matchIntegrationSource
                     << ") rule matched (focused + fresh):" << rule.name
                     << "proc=" << window.processName;
            return rule;
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

    // Strip a dangling separator left when a combined template like
    // "RuneLight – {{runelite.activity}}" resolves with an empty variable
    // (e.g. the RuneLite plugin isn't feeding activity) → "RuneLight" not
    // "RuneLight – ".
    auto tidy = [](QString s) {
        s = s.trimmed();
        static const QRegularExpression trail(QStringLiteral("\\s*[\\x{2013}\\x{2014}\\-|:\\x{00B7}]+\\s*$"));
        static const QRegularExpression lead(QStringLiteral("^\\s*[\\x{2013}\\x{2014}\\-|:\\x{00B7}]+\\s*"));
        s.remove(trail);
        s.remove(lead);
        return s.trimmed();
    };

    PresencePayload p;
    p.activityType   = rule.activityType;
    p.name           = tidy(TemplateEngine::render(rule.activityNameTemplate, ctx));
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

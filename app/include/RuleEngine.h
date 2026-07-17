// RuleEngine.h — Evaluates the deterministic priority chain and produces a PresencePayload.
//
// Priority order (highest first):
//  0. Input-idle override (AFK / Away-from-computer) — see IdleConfig below
//  1. Manual pause / private override
//  2. Manual pinned presence
//  3. Deep integration context for the current active app
//  4. Specific user rule (matched by process/title/domain/integration)
//  5. Browser sanitised domain/category rule
//  6. Generic process rule
//  7. Private fallback (name="Computer", details="Working privately", state="Private")
#pragma once

#include "PresencePayload.h"
#include "Rule.h"
#include "WindowInfo.h"
#include "IntegrationContext.h"
#include <optional>

namespace OmniPresence {

/// Immutable snapshot of the manual override state fed from AppController.
struct ManualOverrideState {
    bool               paused{false};
    bool               privateMode{false};
    std::optional<PresencePayload> pinnedPresence;   ///< Priority-2 pin.
};

/// Idle-tier override config (system input-idle -> AFK / Away-from-computer).
/// Two tiers, higher threshold checked first: idle >= awaySeconds in ANY app
/// -> Away (drops app identity entirely); idle >= afkSeconds while RuneLite is
/// the focused window -> AFK (keeps the OSRS icon). `enabled` defaults to
/// false here so a caller that omits idle arguments to evaluate() (existing
/// call sites / tests) gets the pre-Phase-3 behaviour unchanged; ConfigStore
/// applies the real shipped defaults (enabled=true, 120 s / 600 s, …) when
/// parsing config/omnipresence.json. idleSeconds itself is measured by
/// InputIdleMonitor (Win32 GetLastInputInfo — duration only, see its header
/// for the privacy constraint).
struct IdleConfig {
    bool    enabled{false};
    quint64 afkSeconds{120};
    quint64 awaySeconds{600};
    QString afkLabel{QStringLiteral("AFK")};
    QString awayLabel{QStringLiteral("Away from computer")};
    QString awayImageKey;
};

class RuleEngine {
public:
    RuleEngine() = default;

    /// Evaluate all inputs and return the winning PresencePayload.
    ///
    /// @param window          Currently focused window.
    /// @param integrations    Latest integration context.
    /// @param rules           User-defined rule set (sorted by priority externally or here).
    /// @param overrideState   Manual pause/pin state.
    /// @param previousPayload The previously published payload (used for Keep timestamp mode).
    /// @param idleSeconds     Seconds since the last keyboard/mouse input (0 if unknown/unused).
    /// @param focusedProcessName Process name of the currently focused window (e.g. "RuneLite.exe").
    /// @param idle            Idle-tier override config; enabled=false (the struct default)
    ///                        means this parameter never changes behaviour.
    [[nodiscard]] PresencePayload evaluate(
        const WindowInfo&           window,
        const IntegrationContext&   integrations,
        const RuleSet&              rules,
        const ManualOverrideState&  overrideState,
        const PresencePayload&      previousPayload,
        quint64                     idleSeconds = 0,
        const QString&              focusedProcessName = QString(),
        const IdleConfig&           idle = IdleConfig()) const;

    /// Build a generic, title-safe presence for an unmatched window when private
    /// mode is OFF — shows the friendly app name (never the window title) so the
    /// user sees real activity instead of the private fallback. Public so the UI
    /// can derive a friendly main line when seeding a rule from a captured window.
    [[nodiscard]] static PresencePayload genericPresence(const WindowInfo& window);

private:
    /// Try to match rules at a given specificity level.
    /// Returns the first matching rule or nullopt.
    [[nodiscard]] std::optional<Rule> matchRule(
        const QList<Rule>&          sortedRules,
        const WindowInfo&           window,
        const IntegrationContext&   integrations,
        bool                        requireIntegration,
        bool                        genericProcessOnly) const;

    /// Build the private fallback payload (used when paused or private mode is on).
    [[nodiscard]] static PresencePayload privateFallback();

    /// Build the "Away from computer" payload (idle >= awaySeconds, any app —
    /// drops app identity entirely). Mirrors privateFallback()'s construction.
    [[nodiscard]] static PresencePayload awayPresence(const IdleConfig& idle);

    /// Build the "OSRS / AFK" payload (idle >= afkSeconds, RuneLite focused —
    /// keeps the OSRS icon). `largeImageKey` is resolved by the caller, either
    /// from the matched RuneLite rule or a hardcoded fallback (see runeliteIconKey
    /// in RuleEngine.cpp).
    [[nodiscard]] static PresencePayload afkPresence(const IdleConfig& idle, const QString& largeImageKey);

    /// Resolve templates and fill a PresencePayload from a matched Rule.
    [[nodiscard]] PresencePayload resolveRule(
        const Rule&                 rule,
        const WindowInfo&           window,
        const IntegrationContext&   integrations,
        const PresencePayload&      previousPayload) const;
};

} // namespace OmniPresence

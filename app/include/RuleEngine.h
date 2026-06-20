// RuleEngine.h — Evaluates the deterministic priority chain and produces a PresencePayload.
//
// Priority order (highest first):
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
    [[nodiscard]] PresencePayload evaluate(
        const WindowInfo&           window,
        const IntegrationContext&   integrations,
        const RuleSet&              rules,
        const ManualOverrideState&  overrideState,
        const PresencePayload&      previousPayload) const;

private:
    /// Try to match rules at a given specificity level.
    /// Returns the first matching rule or nullopt.
    [[nodiscard]] std::optional<Rule> matchRule(
        const QList<Rule>&          sortedRules,
        const WindowInfo&           window,
        const IntegrationContext&   integrations,
        bool                        requireIntegration,
        bool                        genericProcessOnly) const;

    /// Build the private fallback payload.
    [[nodiscard]] static PresencePayload privateFallback();

    /// Resolve templates and fill a PresencePayload from a matched Rule.
    [[nodiscard]] PresencePayload resolveRule(
        const Rule&                 rule,
        const WindowInfo&           window,
        const IntegrationContext&   integrations,
        const PresencePayload&      previousPayload) const;
};

} // namespace OmniPresence

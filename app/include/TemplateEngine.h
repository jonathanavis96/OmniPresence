// TemplateEngine.h — Renders {{variable}} templates against a context map.
//
// Supported variables (see CONTRACT):
//   {{app.name}}  {{window.title}}  {{process.name}}
//   {{browser.domain}}  {{browser.category}}
//   {{terminal.cwd}}  {{terminal.repo}}  {{terminal.command_summary}}
//   {{vscode.workspace}}
//   {{runelite.activity}}  {{runelite.target}}  {{runelite.skill}}
//   {{runelite.location}}  {{runelite.confidence}}
//   ({{runelite.activity}} is routed through omni::skillLabel() — see
//   SkillLabel.h — so a recognised OSRS skill renders as "Training {Skill}";
//   non-skill activity passes through verbatim.)
//   {{poe.zone}}  {{poe.zoneCategory}}  {{poe.activity}}  {{poe.character}}
//   {{poe.characterClass}}  {{poe.level}}  {{poe.deaths}}  {{poe.afk}}  {{poe.focused}}
//   {{poe.state}} — derived: "<zone> — Level <N> <Class>" once a configured
//   character has levelled up this session, else just the bare zone.
//
// Fallback syntax:
//   {{terminal.repo or vscode.workspace}}  — first non-empty value wins.
//   Tokens separated by " or " inside the braces.
#pragma once

#include <QString>
#include <QMap>

namespace OmniPresence {

struct WindowInfo;  // defined as a struct in WindowInfo.h
class IntegrationContext;

using TemplateContext = QMap<QString, QString>;

class TemplateEngine {
public:
    TemplateEngine() = default;

    /// Build a context map from the current window + integration data.
    static TemplateContext buildContext(const WindowInfo&         window,
                                       const IntegrationContext& integrations);

    /// Render a template string against a pre-built context map.
    /// Unresolved variables are replaced with an empty string.
    static QString render(const QString& tmpl, const TemplateContext& ctx);

    /// Convenience overload that builds the context internally.
    static QString render(const QString& tmpl,
                          const WindowInfo& window,
                          const IntegrationContext& integrations);

private:
    /// Resolve a single variable token (may contain " or " fallbacks).
    static QString resolveToken(const QString& token, const TemplateContext& ctx);
};

} // namespace OmniPresence

// TemplateEngine.cpp — {{variable}} and {{a or b}} template resolution.
#include "TemplateEngine.h"
#include "WindowInfo.h"
#include "IntegrationContext.h"
#include "SkillLabel.h"
#include <QRegularExpression>
#include <QStringList>

namespace OmniPresence {

// ── Context building ──────────────────────────────────────────────────────────

TemplateContext TemplateEngine::buildContext(const WindowInfo& window,
                                            const IntegrationContext& integrations)
{
    TemplateContext ctx;

    // Window variables
    ctx[QStringLiteral("app.name")]    = window.processName;
    ctx[QStringLiteral("window.title")]= window.windowTitle;
    ctx[QStringLiteral("process.name")]= window.processName;
    // window.doctitle — the document/tab name only: the window title's first
    // segment before a " - " / " — " / " | " / " · " separator, so e.g.
    // "report.txt - Notepad" -> "report.txt" and
    // "MyNote - MyVault - Obsidian v1.5" -> "MyNote". Falls back to the full
    // title when there is no separator (e.g. Explorer's bare "Downloads").
    {
        const QString wt = window.windowTitle;
        static const QRegularExpression sep(
            QStringLiteral("\\s+[\\x{2013}\\x{2014}\\-|\\x{00B7}]\\s+"));
        QString doc = wt.section(sep, 0, 0).trimmed();
        if (doc.isEmpty()) doc = wt.trimmed();
        // Drop a leading unsaved-changes marker (Notepad shows "*file.txt").
        if (doc.startsWith(QLatin1Char('*'))) doc = doc.mid(1).trimmed();
        ctx[QStringLiteral("window.doctitle")] = doc;
    }

    // Integration fields, exposed as "<token>" from <source>'s payload <key> — empty
    // unless that source's payload is still fresh.
    static const struct { const char* token; const char* source; const char* key; } kFields[] = {
        {"browser.domain",           "browser",  "domain"},
        {"browser.category",         "browser",  "category"},
        {"browser.title",            "browser",  "safe_title"},   // whitelisted tab title only
        {"browser.label",            "browser",  "page_label"},
        {"terminal.cwd",             "terminal", "cwd"},
        {"terminal.repo",            "terminal", "repo"},
        {"terminal.command_summary", "terminal", "command_summary"},
        {"vscode.workspace",         "vscode",   "workspace"},
        {"runelite.activity",        "runelite", "activity"},
        {"runelite.target",          "runelite", "target"},
        {"runelite.skill",           "runelite", "skill"},
        {"runelite.location",        "runelite", "location"},
        {"runelite.confidence",      "runelite", "confidence"},
        {"poe.zone",                 "poe",      "zone"},
        {"poe.zoneCategory",         "poe",      "zoneCategory"},
        {"poe.activity",             "poe",      "activity"},
        {"poe.character",            "poe",      "character"},
        {"poe.characterClass",       "poe",      "characterClass"},
        {"poe.level",                "poe",      "level"},
        {"poe.deaths",               "poe",      "deaths"},
        {"poe.afk",                  "poe",      "afk"},
        {"poe.focused",              "poe",      "focused"},
    };
    for (const auto& f : kFields)
        ctx[QLatin1String(f.token)] =
            integrations.freshField(QLatin1String(f.source), QLatin1String(f.key));

    // browser.site — the site's name from its domain (dotabuff.com -> "Dotabuff").
    {
        QString site = ctx.value(QStringLiteral("browser.domain")).section(QLatin1Char('.'), 0, 0);
        if (!site.isEmpty()) site[0] = site[0].toUpper();
        ctx[QStringLiteral("browser.site")] = site;
    }

    // runelite.activity is routed through omni::skillLabel so a recognised OSRS
    // skill (e.g. the built-in Discord plugin's "Training: Mining") renders as
    // "Training Mining"/"Training Runecrafting"; non-skill activity (bosses,
    // minigames) passes through verbatim.
    ctx[QStringLiteral("runelite.activity")] =
        omni::skillLabel(ctx.value(QStringLiteral("runelite.activity")));

    // poe.state — "<zone> — Level <N> <Class>" once a configured character has
    // levelled up this session, else just the bare zone. Character identity is
    // omitted unless the user opted in (see design doc), so this must degrade
    // to zone-only rather than leaving a dangling "— Level  " fragment.
    {
        const QString zone  = ctx.value(QStringLiteral("poe.zone"));
        const QString klass = ctx.value(QStringLiteral("poe.characterClass"));
        const QString level = ctx.value(QStringLiteral("poe.level"));
        QString state = zone;
        if (!klass.isEmpty() && !level.isEmpty()) {
            state = zone.isEmpty()
                ? QStringLiteral("Level %1 %2").arg(level, klass)
                : QStringLiteral("%1 — Level %2 %3").arg(zone, level, klass);
        }
        ctx[QStringLiteral("poe.state")] = state;
    }

    return ctx;
}

// ── Token resolution ──────────────────────────────────────────────────────────

QString TemplateEngine::resolveToken(const QString& token, const TemplateContext& ctx) {
    // Support "{{a or b or c}}" — split on " or " and return first non-empty value.
    const QStringList parts = token.split(QStringLiteral(" or "));
    for (const QString& part : parts) {
        const QString key   = part.trimmed();
        const auto    it    = ctx.constFind(key);
        if (it != ctx.constEnd() && !it.value().isEmpty()) {
            return it.value();
        }
    }
    return {};
}

// ── Render ────────────────────────────────────────────────────────────────────

QString TemplateEngine::render(const QString& tmpl, const TemplateContext& ctx) {
    if (tmpl.isEmpty()) return {};

    static const QRegularExpression kVarPattern(QStringLiteral(R"(\{\{([^}]+)\}\})"));

    QString result = tmpl;
    QRegularExpressionMatch match;
    int offset = 0;
    while ((match = kVarPattern.match(result, offset)).hasMatch()) {
        const QString full  = match.captured(0);
        const QString token = match.captured(1).trimmed();
        const QString value = resolveToken(token, ctx);
        result.replace(match.capturedStart(), full.length(), value);
        offset = match.capturedStart() + value.length();
    }
    return result;
}

QString TemplateEngine::render(const QString& tmpl,
                               const WindowInfo& window,
                               const IntegrationContext& integrations)
{
    return render(tmpl, buildContext(window, integrations));
}

} // namespace OmniPresence

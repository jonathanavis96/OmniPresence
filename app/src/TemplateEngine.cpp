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

    // Browser
    ctx[QStringLiteral("browser.domain")]   = integrations.browserDomain();
    ctx[QStringLiteral("browser.category")] = integrations.browserCategory();
    ctx[QStringLiteral("browser.title")]    = integrations.browserTitle();
    ctx[QStringLiteral("browser.label")]    = integrations.browserLabel();
    // browser.site — the site's name from its domain (dotabuff.com -> "Dotabuff").
    {
        const QString dom = integrations.browserDomain();
        QString site = dom.section(QLatin1Char('.'), 0, 0);  // first label
        if (!site.isEmpty()) site[0] = site[0].toUpper();
        ctx[QStringLiteral("browser.site")] = site;
    }

    // Terminal
    ctx[QStringLiteral("terminal.cwd")]             = integrations.terminalCwd();
    ctx[QStringLiteral("terminal.repo")]            = integrations.terminalRepo();
    ctx[QStringLiteral("terminal.command_summary")] = integrations.terminalCommandSummary();

    // VS Code
    ctx[QStringLiteral("vscode.workspace")] = integrations.vscodeWorkspace();

    // RuneLite
    // runelite.activity is routed through omni::skillLabel so a recognised OSRS
    // skill (e.g. the built-in Discord plugin's "Training: Mining") renders as
    // "Training Mining"/"Training Runecrafting"; non-skill activity (bosses,
    // minigames) passes through verbatim.
    ctx[QStringLiteral("runelite.activity")]   = omni::skillLabel(integrations.runeliteActivity());
    ctx[QStringLiteral("runelite.target")]     = integrations.runeliteTarget();
    ctx[QStringLiteral("runelite.skill")]      = integrations.runeliteSkill();
    ctx[QStringLiteral("runelite.location")]   = integrations.runeliteLocation();
    ctx[QStringLiteral("runelite.confidence")] = integrations.runeliteConfidence();

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

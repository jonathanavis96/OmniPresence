// test_template_engine.cpp — template fallback chain, incl. terminal-title fix.
#include <QtTest>
#include "TemplateEngine.h"

using namespace OmniPresence;

class TestTemplateEngine : public QObject {
    Q_OBJECT
private slots:
    void terminalFallsBackToWindowTitle() {
        TemplateContext ctx;
        ctx[QStringLiteral("terminal.repo")]    = QString();   // hook not running
        ctx[QStringLiteral("vscode.workspace")] = QString();
        ctx[QStringLiteral("window.title")]     = QStringLiteral("RAM");
        const QString out = TemplateEngine::render(
            QStringLiteral("Working on {{terminal.repo or vscode.workspace or window.title}}"), ctx);
        QCOMPARE(out, QStringLiteral("Working on RAM"));
    }

    void prefersRepoWhenPresent() {
        TemplateContext ctx;
        ctx[QStringLiteral("terminal.repo")] = QStringLiteral("OmniPresence");
        ctx[QStringLiteral("window.title")]  = QStringLiteral("RAM");
        const QString out = TemplateEngine::render(
            QStringLiteral("Working on {{terminal.repo or vscode.workspace or window.title}}"), ctx);
        QCOMPARE(out, QStringLiteral("Working on OmniPresence"));
    }
};

QTEST_MAIN(TestTemplateEngine)
#include "test_template_engine.moc"

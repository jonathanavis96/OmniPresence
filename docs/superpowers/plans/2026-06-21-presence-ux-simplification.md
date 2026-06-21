# Presence UX Simplification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make OmniPresence's day-to-day UX match real use: a combined "name – activity" main line, a faithful presence preview, a working "create rule from window", a simplified rule form, one-click Add photo, and a terminal-title fallback.

**Architecture:** C++20/Qt6/QML. `AppController` bridges backend → QML via `Q_PROPERTY`/`Q_INVOKABLE`. `ConfigStore` persists a flat JSON config. `RuleEngine` + `TemplateEngine` render a `PresencePayload` that `DiscordPresenceClient` publishes. This plan stands up a QtTest harness for the pure-logic pieces, wires the currently-placeholder Rules/Asset QML pages to real backend models, and adds a local art store.

**Tech Stack:** C++20, Qt 6.8 (Core/Gui/Qml/Quick/Test), CMake + Ninja, MSVC2022, Discord Social SDK (`discord_partner_sdk`).

## Global Constraints

- Target runtime: **Windows 11**. Build via `build-discord.bat` (clean) or `rebuild-inc.bat` (incremental). After any **clean** rebuild, re-run `windeployqt --qmldir app\qml build-discord\app\omnipresence.exe`.
- **No Python at runtime** — image work uses `QImage`.
- **No Playwright** for art upload — Add photo saves locally + opens the portal for a manual drop.
- The preview and the published payload MUST read the **same** `PresencePayload` (single source of truth).
- Member-list compact line = activity **Name** for every rule (`PresencePayload::statusDisplay = StatusDisplay::Name`).
- Discord art assets: lowercase keys, normalize images to **1024×1024 PNG**.
- After code changes run `graphify update .`. Commit trailer (personal repo):
  `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>` + `Claude-Session: …`.
- Existing enums/structs (do not redefine): `Rule`, `RuleSet`, `PresencePayload`, `StatusDisplay{Name,State,Details}`, `PrivacyLevel{Public,DomainOnly,Private}`, `ActivityType`, `TimestampMode`.

---

## File Structure

- `tests/CMakeLists.txt` — **new**, QtTest target `omnipresence_tests` linking the app's logic sources.
- `tests/test_template_engine.cpp` — **new**, fallback-chain + terminal-title tests.
- `tests/test_art_store.cpp` — **new**, slug/dedupe/normalize/round-trip tests.
- `tests/test_rule_engine_render.cpp` — **new**, main-line + RuneScape render tests.
- `app/include/ArtStore.h` / `app/src/ArtStore.cpp` — **new**, local art store + image normalize + slug.
- `app/src/ConfigStore.cpp` (+ `.h`) — **modify**, persist `assetKeys`; idempotent terminal-template migration.
- `app/include/AppController.h` / `app/src/AppController.cpp` — **modify**, art sources, `availableContextFields`, rule CRUD invokables, `seedRuleFromCapture`, `addPhoto`.
- `app/src/RuleEngine.cpp` — **modify**, force `statusDisplay = Name`.
- `config/omnipresence.example.json` — **modify**, combined RuneScape main line + terminal `or window.title`.
- `app/qml/PreviewPage.qml` — **modify**, mirror real payload + art.
- `app/qml/RulesPage.qml` — **modify**, simplified form + Advanced collapse, bound to real model.
- `app/qml/CapturePage.qml` — **modify**, call `seedRuleFromCapture()`.
- `app/qml/AssetManager.qml` — **modify**, Add photo button + local thumbnails from art store.
- `scripts/make-placeholder-art.cpp` (or reuse ArtStore) — **new**, committed placeholder-art generator.

---

### Task 1: QtTest harness + terminal-title fallback (bug 6)

**Files:**
- Create: `tests/CMakeLists.txt`, `tests/test_template_engine.cpp`
- Modify: `CMakeLists.txt:27-31` (already conditionally adds `tests/` — no change needed if guard present)
- Modify: `config/omnipresence.example.json:58-59` (terminal rule templates)
- Modify: `app/src/ConfigStore.cpp` (idempotent migration in `parseJson`)

**Interfaces:**
- Consumes: `TemplateEngine::render(const QString&, const TemplateContext&)`, `TemplateEngine::resolveToken` (TemplateEngine.cpp:46), `TemplateContext` (QHash<QString,QString>).
- Produces: test target `omnipresence_tests`; migration `ConfigStore::migrateRuleTemplates()` (private, called at end of `parseJson`).

- [ ] **Step 1: Write `tests/CMakeLists.txt`**

```cmake
# tests/CMakeLists.txt — QtTest harness for pure-logic units.
find_package(Qt6 REQUIRED COMPONENTS Test Core Gui)

add_executable(omnipresence_tests
    test_template_engine.cpp
    ${CMAKE_SOURCE_DIR}/app/src/TemplateEngine.cpp
    ${CMAKE_SOURCE_DIR}/app/src/IntegrationContext.cpp
    ${CMAKE_SOURCE_DIR}/app/src/Rule.cpp
)
target_include_directories(omnipresence_tests PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(omnipresence_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME omnipresence_tests COMMAND omnipresence_tests)
```

- [ ] **Step 2: Write the failing test `tests/test_template_engine.cpp`**

```cpp
#include <QtTest>
#include "TemplateEngine.h"
using namespace OmniPresence;

class TestTemplateEngine : public QObject {
    Q_OBJECT
private slots:
    void terminalFallsBackToWindowTitle() {
        TemplateContext ctx;
        ctx["terminal.repo"]      = "";        // hook not running
        ctx["vscode.workspace"]   = "";
        ctx["window.title"]       = "RAM";     // real Windows Terminal title
        const QString out = TemplateEngine::render(
            "Working on {{terminal.repo or vscode.workspace or window.title}}", ctx);
        QCOMPARE(out, QString("Working on RAM"));
    }
    void prefersRepoWhenPresent() {
        TemplateContext ctx;
        ctx["terminal.repo"] = "OmniPresence";
        ctx["window.title"]  = "RAM";
        const QString out = TemplateEngine::render(
            "Working on {{terminal.repo or vscode.workspace or window.title}}", ctx);
        QCOMPARE(out, QString("Working on OmniPresence"));
    }
};
QTEST_MAIN(TestTemplateEngine)
#include "test_template_engine.moc"
```

- [ ] **Step 3: Build + run, expect PASS** (engine already supports the chain — this is a guard test)

Run (Windows): `cmake --build build-discord --target omnipresence_tests && build-discord\tests\omnipresence_tests.exe`
Expected: `Totals: 2 passed`. If `window.title` is not in the context, the engine change is unnecessary — the bug is purely the rule template.

- [ ] **Step 4: Fix the example config terminal rule**

In `config/omnipresence.example.json`, change the terminal rule:
```json
"detailsTemplate": "Working on {{terminal.repo or vscode.workspace or window.title}}",
"stateTemplate": "{{terminal.command_summary or vscode.current_task or window.title}}",
```

- [ ] **Step 5: Add idempotent migration so the live config is fixed too**

In `ConfigStore.cpp`, add a private helper and call it at the end of `parseJson` (before `return true;`):
```cpp
void ConfigStore::migrateRuleTemplates() {
    // Ensure terminal/code rules fall back to the window title when no
    // integration context is present (fixes "Working on " blank). Idempotent.
    bool changed = false;
    auto rules = m_ruleSet.rules();           // copy
    for (Rule& r : rules) {
        if (r.matchIntegrationSource == QLatin1String("terminal")
            && !r.detailsTemplate.contains(QLatin1String("window.title"))
            && r.detailsTemplate.contains(QLatin1String("{{"))) {
            r.detailsTemplate.replace(QLatin1String("}}"),
                                      QLatin1String(" or window.title}}"));
            m_ruleSet.updateRule(r);
            changed = true;
        }
    }
    if (changed) qDebug() << "[ConfigStore] migrated terminal rule templates";
}
```
Declare `void migrateRuleTemplates();` in the private section of `ConfigStore.h`. Call `migrateRuleTemplates();` after `m_ruleSet = …` in `parseJson`.

- [ ] **Step 6: Commit**

```bash
git add tests/CMakeLists.txt tests/test_template_engine.cpp config/omnipresence.example.json app/src/ConfigStore.cpp app/include/ConfigStore.h
git commit -m "Add QtTest harness; terminal rule falls back to window title"
```

---

### Task 2: ArtStore — local art, image normalize, slug/dedupe

**Files:**
- Create: `app/include/ArtStore.h`, `app/src/ArtStore.cpp`, `tests/test_art_store.cpp`
- Modify: `app/CMakeLists.txt` (add ArtStore.cpp to app sources), `tests/CMakeLists.txt` (add test)

**Interfaces:**
- Produces:
  - `QString ArtStore::slugify(const QString& raw)` → lowercase `[a-z0-9_]`, deduped against existing keys is the caller's job; slugify is pure.
  - `bool ArtStore::importImage(const QString& srcPath, const QString& key, QString* outPath, QString* err)` → normalizes to 1024×1024 PNG, writes `<artDir>/<key>.png`, returns local path.
  - `QString ArtStore::artDir()` → `%APPDATA%/OmniPresence/art` (created on first use).
  - `QString ArtStore::localPathForKey(const QString& key)` → existing file or "".

- [ ] **Step 1: Write the failing test `tests/test_art_store.cpp`**

```cpp
#include <QtTest>
#include <QImage>
#include <QTemporaryDir>
#include "ArtStore.h"
using namespace OmniPresence;

class TestArtStore : public QObject {
    Q_OBJECT
private slots:
    void slugifyLowercasesAndStrips() {
        QCOMPARE(ArtStore::slugify("My Photo!.png"), QString("my_photo"));
        QCOMPARE(ArtStore::slugify("OSRS"),          QString("osrs"));
    }
    void importNormalisesTo1024Png() {
        QTemporaryDir tmp;
        const QString src = tmp.filePath("in.png");
        QImage img(200, 80, QImage::Format_RGB32); img.fill(Qt::cyan);
        QVERIFY(img.save(src));
        ArtStore store(tmp.filePath("art"));      // test override dir
        QString out, err;
        QVERIFY2(store.importImage(src, "test", &out, &err), qPrintable(err));
        QImage got(out);
        QCOMPARE(got.width(), 1024);
        QCOMPARE(got.height(), 1024);
        QCOMPARE(store.localPathForKey("test"), out);
    }
};
QTEST_MAIN(TestArtStore)
#include "test_art_store.moc"
```

- [ ] **Step 2: Run, expect FAIL** (ArtStore.h missing)

Run: `cmake --build build-discord --target omnipresence_tests`
Expected: compile error `ArtStore.h: No such file`.

- [ ] **Step 3: Write `app/include/ArtStore.h`**

```cpp
#pragma once
#include <QString>
namespace OmniPresence {
class ArtStore {
public:
    ArtStore();                         ///< uses %APPDATA%/OmniPresence/art
    explicit ArtStore(QString dir);     ///< explicit dir (tests)
    static QString slugify(const QString& raw);
    QString artDir() const { return m_dir; }
    QString localPathForKey(const QString& key) const;
    bool importImage(const QString& srcPath, const QString& key,
                     QString* outPath, QString* err) const;
private:
    QString m_dir;
};
} // namespace OmniPresence
```

- [ ] **Step 4: Write `app/src/ArtStore.cpp`**

```cpp
#include "ArtStore.h"
#include <QDir>
#include <QImage>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>
namespace OmniPresence {

ArtStore::ArtStore()
    : m_dir(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("art"))) {}
ArtStore::ArtStore(QString dir) : m_dir(std::move(dir)) {}

QString ArtStore::slugify(const QString& raw) {
    QString base = QFileInfo(raw).completeBaseName().toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    base = base.mid(0).remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    return base.isEmpty() ? QStringLiteral("art") : base;
}

QString ArtStore::localPathForKey(const QString& key) const {
    const QString p = QDir(m_dir).filePath(key + QStringLiteral(".png"));
    return QFileInfo::exists(p) ? p : QString();
}

bool ArtStore::importImage(const QString& srcPath, const QString& key,
                           QString* outPath, QString* err) const {
    QImage img(srcPath);
    if (img.isNull()) { if (err) *err = QStringLiteral("Unreadable image"); return false; }
    QImage square = img.scaled(1024, 1024, Qt::KeepAspectRatioByExpanding,
                               Qt::SmoothTransformation)
                       .copy(0, 0, 1024, 1024);
    QDir().mkpath(m_dir);
    const QString p = QDir(m_dir).filePath(key + QStringLiteral(".png"));
    if (!square.save(p, "PNG")) { if (err) *err = QStringLiteral("Cannot write %1").arg(p); return false; }
    if (outPath) *outPath = p;
    return true;
}
} // namespace OmniPresence
```

- [ ] **Step 5: Add to build** — append `ArtStore.cpp` to the app target sources in `app/CMakeLists.txt`, and to `tests/CMakeLists.txt`:
```cmake
add_executable(omnipresence_tests
    test_template_engine.cpp
    test_art_store.cpp
    ${CMAKE_SOURCE_DIR}/app/src/TemplateEngine.cpp
    ${CMAKE_SOURCE_DIR}/app/src/IntegrationContext.cpp
    ${CMAKE_SOURCE_DIR}/app/src/Rule.cpp
    ${CMAKE_SOURCE_DIR}/app/src/ArtStore.cpp
)
```
(QtTest auto-runs both `QTEST_MAIN`? No — one main per exe. Split into two test exes OR use `QTEST_APPLESS_MAIN`-free combine. Simplest: give each test file its own executable.) Replace the single target with two:
```cmake
foreach(t test_template_engine test_art_store)
    add_executable(${t} ${t}.cpp
        ${CMAKE_SOURCE_DIR}/app/src/TemplateEngine.cpp
        ${CMAKE_SOURCE_DIR}/app/src/IntegrationContext.cpp
        ${CMAKE_SOURCE_DIR}/app/src/Rule.cpp
        ${CMAKE_SOURCE_DIR}/app/src/ArtStore.cpp)
    target_include_directories(${t} PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
    target_link_libraries(${t} PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
    add_test(NAME ${t} COMMAND ${t})
endforeach()
```

- [ ] **Step 6: Build + run, expect PASS**

Run: `cmake --build build-discord --target test_art_store && build-discord\tests\test_art_store.exe`
Expected: `Totals: 2 passed`.

- [ ] **Step 7: Commit**

```bash
git add app/include/ArtStore.h app/src/ArtStore.cpp tests/test_art_store.cpp tests/CMakeLists.txt app/CMakeLists.txt
git commit -m "Add ArtStore: local art dir, 1024x1024 PNG normalize, slugify"
```

---

### Task 3: Persist assetKeys (art metadata) in ConfigStore

**Files:**
- Modify: `app/include/ConfigStore.h` (add `QMap<QString,QString> assetKeys` accessor), `app/src/ConfigStore.cpp` (parse/serialise `assetKeys`)
- Test: extend `tests/test_art_store.cpp` or new `tests/test_config_assets.cpp`

**Interfaces:**
- Produces: `const QMap<QString,QString>& ConfigStore::assetKeys() const;` and `void ConfigStore::setAssetKey(const QString& key, const QString& hoverText);` Map = key → hoverText. Local file path is derived from `ArtStore::localPathForKey(key)`, not stored.

- [ ] **Step 1: Write failing test `tests/test_config_assets.cpp`** (round-trip)

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "ConfigStore.h"
using namespace OmniPresence;
class TestConfigAssets : public QObject {
    Q_OBJECT
private slots:
    void assetKeysRoundTrip() {
        QTemporaryDir tmp;
        ConfigStore a; a.setConfigPathForTest(tmp.filePath("c.json"));
        a.setAssetKey("osrs", "Playing OSRS");
        QVERIFY(a.save());
        ConfigStore b; b.setConfigPathForTest(tmp.filePath("c.json"));
        QVERIFY(b.load());
        QCOMPARE(b.assetKeys().value("osrs"), QString("Playing OSRS"));
    }
};
QTEST_MAIN(TestConfigAssets)
#include "test_config_assets.moc"
```
Add `void setConfigPathForTest(const QString& p) { m_configPath = p; }` to ConfigStore.h (test seam).

- [ ] **Step 2: Run, expect FAIL** (`assetKeys`/`setAssetKey` missing).

- [ ] **Step 3: Implement** — in `ConfigStore.h` add `QMap<QString,QString> m_assetKeys;` + accessors. In `ConfigStore.cpp`:
  - In `parseJson`, after rules: `const QJsonObject ak = root.value("assetKeys").toObject(); m_assetKeys.clear(); for (auto it = ak.begin(); it != ak.end(); ++it) m_assetKeys.insert(it.key(), it.value().toString());`
  - In `serialiseJson`, before return: `QJsonObject ak; for (auto it = m_assetKeys.begin(); it != m_assetKeys.end(); ++it) ak[it.key()] = it.value(); root["assetKeys"] = ak;`

- [ ] **Step 4: Add test to `tests/CMakeLists.txt` loop list, build, run — expect PASS.**

- [ ] **Step 5: Commit**

```bash
git add app/include/ConfigStore.h app/src/ConfigStore.cpp tests/test_config_assets.cpp tests/CMakeLists.txt
git commit -m "Persist assetKeys (key -> hover text) in config"
```

---

### Task 4: RuleEngine — force Name display; combined RuneScape main line

**Files:**
- Modify: `app/src/RuleEngine.cpp` (set `payload.statusDisplay = StatusDisplay::Name` for all rule-based + generic payloads)
- Modify: `config/omnipresence.example.json` (RuneScape rule: combined main line)
- Test: `tests/test_rule_engine_render.cpp`

**Interfaces:**
- Consumes: `RuleEngine::evaluate(...)` producing `PresencePayload` (existing). Confirm the exact evaluate signature in `RuleEngine.h` before writing the test; the test calls it with a `WindowInfo` + `IntegrationContext` carrying `runelite.activity="Training Crafting"`, `runelite.location="Grand Exchange"`.

- [ ] **Step 1: Write failing test** asserting, for the RuneScape rule with the integration context above:
```cpp
QCOMPARE(p.name,  QString("RuneLight – Training Crafting")); // en dash
QCOMPARE(p.state, QString("Grand Exchange"));
QCOMPARE(int(p.statusDisplay), int(StatusDisplay::Name));
```
(Load the rule from `config/omnipresence.example.json` or construct it inline — construct inline to avoid file path issues in tests.)

- [ ] **Step 2: Run, expect FAIL.**

- [ ] **Step 3: Implement** —
  - In `RuleEngine.cpp` wherever the rule-based payload and `genericPresence()` are built, set `payload.statusDisplay = StatusDisplay::Name;` (remove the prior "Details when details non-empty" logic noted in vault 2026-06-21).
  - In `config/omnipresence.example.json` RuneScape rule:
    ```json
    "activityNameTemplate": "RuneLight – {{runelite.activity}}",
    "detailsTemplate": "",
    "stateTemplate": "{{runelite.location}}",
    ```

- [ ] **Step 4: Run, expect PASS.**

- [ ] **Step 5: Commit**

```bash
git add app/src/RuleEngine.cpp config/omnipresence.example.json tests/test_rule_engine_render.cpp tests/CMakeLists.txt
git commit -m "Main line = activity Name (Name display); combined RuneScape line"
```

---

### Task 5: AppController — art sources, available context, exact-payload exposure

**Files:**
- Modify: `app/include/AppController.h`, `app/src/AppController.cpp`

**Interfaces:**
- Produces (QML-visible):
  - `Q_PROPERTY(QString presenceLargeImageSource …)` / `presenceSmallImageSource` — `"file:///<artDir>/<key>.png"` if local, else `"qrc:/OmniPresence/resources/assets/<key>.png"` if bundled, else `""`.
  - `Q_PROPERTY(QVariantList availableContextFields …)` — list of `{token, label}` for the current window (only non-empty context vars).
- Internal: reuses `m_lastPublishedPresence` / `m_currentPresence` (already members, AppController.h:125-126) as the single source for `presenceName/Details/State`.

- [ ] **Step 1:** Add an `ArtStore m_artStore;` member and a helper:
```cpp
QString AppController::sourceForKey(const QString& key) const {
    if (key.isEmpty()) return {};
    const QString local = m_artStore.localPathForKey(key);
    if (!local.isEmpty()) return QUrl::fromLocalFile(local).toString();
    return QStringLiteral("qrc:/OmniPresence/resources/assets/") + key + QStringLiteral(".png");
}
QString AppController::presenceLargeImageSource() const { return sourceForKey(m_currentPresence.largeImageKey); }
QString AppController::presenceSmallImageSource() const { return sourceForKey(m_currentPresence.smallImageKey); }
```

- [ ] **Step 2:** Add `availableContextFields()`:
```cpp
QVariantList AppController::availableContextFields() const {
    const auto ctx = TemplateEngine::buildContext(m_currentWindow, m_integrationContext);
    static const QVector<QPair<QString,QString>> known = {
        {"window.title", "The window / tab title"},
        {"browser.domain", "The website domain"},
        {"runelite.activity", "RuneScape activity"},
        {"runelite.location", "RuneScape location"},
        {"terminal.repo", "Terminal repository"},
        {"vscode.workspace", "VS Code workspace"},
    };
    QVariantList out;
    for (const auto& k : known)
        if (!ctx.value(k.first).isEmpty())
            out.append(QVariantMap{{"token","{{"+k.first+"}}"},{"label",k.second}});
    return out;
}
```
Declare both in `AppController.h` with the `Q_PROPERTY` lines (NOTIFY `presenceChanged` / `windowChanged`).

- [ ] **Step 3: Build the GUI app** (`rebuild-inc.bat`), launch, confirm no QML binding errors in `runlog.txt`. (No unit test — exercised by Task 6/7 manual verify.)

- [ ] **Step 4: Commit**

```bash
git add app/include/AppController.h app/src/AppController.cpp
git commit -m "Expose resolved art sources + availableContextFields to QML"
```

---

### Task 6: Real rule model CRUD exposed to QML

**Files:**
- Modify: `app/include/AppController.h`, `app/src/AppController.cpp`

**Interfaces:**
- Produces (QML invokables, JSON-bridge style to avoid a full QAbstractListModel):
  - `Q_INVOKABLE QVariantList rulesList() const;` → `[{index,name,enabled,priority}]` for the left list.
  - `Q_INVOKABLE QVariantMap ruleAt(int index) const;` → all editable fields of one rule.
  - `Q_INVOKABLE int addRule(const QVariantMap& draft);` → appends, returns new index.
  - `Q_INVOKABLE void updateRuleField(int index, const QString& field, const QVariant& value);`
  - `Q_INVOKABLE void deleteRule(int index);`
  - `Q_INVOKABLE void saveRules();` → writes through ConfigStore, emits `rulesChanged()`.
  - `signal rulesChanged();`
- Consumes: `ConfigStore` rule set (add `RuleSet& ConfigStore::ruleSetMutable()` accessor).

- [ ] **Step 1:** Add `RuleSet& ruleSetMutable();` and `const RuleSet& ruleSet() const;` to ConfigStore.

- [ ] **Step 2:** Implement the invokables in AppController, mapping `QVariantMap` ↔ `Rule` (field names match `Rule` members: `name, enabled, priority, matchProcessName, matchExecutablePath, matchWindowTitle, matchBrowserDomain, matchIntegrationSource, activityNameTemplate, detailsTemplate, stateTemplate, largeImageKey, privacyLevel`). Generate `id` via `QUuid::createUuid().toString(QUuid::WithoutBraces)` when adding.

- [ ] **Step 3:** Build, launch. Manual verify: temporarily log `rulesList().size()` on startup — equals rule count in config. Confirm `saveRules()` writes the file (check `%APPDATA%\OmniPresence\config.json` mtime).

- [ ] **Step 4: Commit**

```bash
git add app/include/AppController.h app/src/AppController.cpp app/include/ConfigStore.h app/src/ConfigStore.cpp
git commit -m "Expose real rule CRUD bridge to QML"
```

---

### Task 7: Simplified rule form (RulesPage.qml)

**Files:**
- Modify: `app/qml/RulesPage.qml` (replace placeholder model + editor)

- [ ] **Step 1:** Bind the left list to `AppController.rulesList()` (refresh on `rulesChanged`). Keep the existing list delegate styling.

- [ ] **Step 2:** Replace the editor body with the simplified fields:
  - **Main line** → `TextField` bound to `activityNameTemplate` (calls `updateRuleField(idx,"activityNameTemplate",text)`).
  - **Public / Private** → `Switch`; checked = Private → `updateRuleField(idx,"privacyLevel", checked ? 2 : 0)`.
  - **Include extra detail?** → `Switch`; when on show a `ComboBox` with `model: AppController.availableContextFields` (textRole `"label"`); selecting sets `updateRuleField(idx,"stateTemplate", model[currentIndex].token)`.
  - **Image** → `ComboBox` of uploaded art keys + an **Add photo** `Button` → `AppController.addPhoto(idx)` (Task 9). Sets `largeImageKey`.

- [ ] **Step 3:** Move all old fields (priority, regex, process/exe/title, browser category, raw templates, timestamp, small image) into a collapsible **Advanced** section (`Button` toggles a `Column` `visible`). Wire each to `updateRuleField`.

- [ ] **Step 4:** "Save" button → `AppController.saveRules()`. "Delete" → `AppController.deleteRule(idx)`.

- [ ] **Step 5: Build + manual verify on Windows.** Evidence to capture (PrintWindow screenshot): only the 4 simple fields show by default; Advanced expands; editing main line + Save persists across relaunch. Note in vault.

- [ ] **Step 6: Commit**

```bash
git add app/qml/RulesPage.qml
git commit -m "Simplify rule editor: main line / public-private / extra detail / image (+ Advanced)"
```

---

### Task 8: Capture → pre-filled simplified form

**Files:**
- Modify: `app/include/AppController.h`, `app/src/AppController.cpp` (`seedRuleFromCapture`), `app/qml/CapturePage.qml`

**Interfaces:**
- Produces: `Q_INVOKABLE int seedRuleFromCapture();` → builds a draft Rule from `m_currentWindow` (matchProcessName = process; activityNameTemplate = friendly app name via the existing `genericPresence` name map; privacyLevel Public), appends it via `addRule`, returns the new index; emits `rulesChanged()`.

- [ ] **Step 1:** Implement `seedRuleFromCapture()` reusing the friendly-name map already in `RuleEngine.cpp:55`/`genericPresence`. If `m_currentWindow.processName` is empty, return -1.

- [ ] **Step 2:** In `CapturePage.qml:86-90`, replace the stub `onClicked` with:
```qml
onClicked: {
    var idx = AppController.seedRuleFromCapture()
    if (idx >= 0) StackView.view.replace(null, Qt.resolvedUrl("RulesPage.qml"), { startIndex: idx })
}
```
Add a `property int startIndex: -1` to RulesPage that pre-selects that rule on load.

- [ ] **Step 3: Build + manual verify:** capture a window → click Create Rule → Rules opens with a new rule pre-selected, main line + process pre-filled. Capture screenshot.

- [ ] **Step 4: Commit**

```bash
git add app/include/AppController.h app/src/AppController.cpp app/qml/CapturePage.qml app/qml/RulesPage.qml
git commit -m "Create Rule from Window now pre-fills the simplified form"
```

---

### Task 9: Add photo flow

**Files:**
- Modify: `app/include/AppController.h`, `app/src/AppController.cpp` (`addPhoto`), `app/qml/RulesPage.qml` / `app/qml/AssetManager.qml` (button + FileDialog)

**Interfaces:**
- Produces: `Q_INVOKABLE void addPhoto(int ruleIndex);` — the QML opens a `FileDialog`; on accept it calls `Q_INVOKABLE QString importPhoto(int ruleIndex, const QString& fileUrl);` which: slugifies a key (deduped against existing art + assetKeys), `ArtStore::importImage`, sets the rule's `largeImageKey`, `ConfigStore::setAssetKey(key, hoverText)`, saves, opens the portal + reveals the file, returns the key.

- [ ] **Step 1:** Implement `importPhoto`:
```cpp
QString AppController::importPhoto(int ruleIndex, const QString& fileUrl) {
    const QString src = QUrl(fileUrl).toLocalFile();
    QString key = ArtStore::slugify(src);
    int n = 1; QString base = key;
    while (!m_artStore.localPathForKey(key).isEmpty()) key = base + QString::number(++n);
    QString out, err;
    if (!m_artStore.importImage(src, key, &out, &err)) { m_discordError = err; emit discordStatusChanged(); return {}; }
    updateRuleField(ruleIndex, "largeImageKey", key);
    m_configStore->setAssetKey(key, key);
    saveRules();
    // open portal + reveal file (no Playwright)
    QDesktopServices::openUrl(QUrl(QStringLiteral(
        "https://discord.com/developers/applications/%1/rich-presence/assets")
        .arg(m_configStore->settings().discordAppId)));
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(out)});
    return key;
}
```

- [ ] **Step 2:** In QML add a `FileDialog` (`import QtQuick.Dialogs`) opened by the **Add photo** button; `onAccepted: AppController.importPhoto(idx, selectedFile)`. Show a Text note: "Drop the file Explorer just revealed into the Art Assets page — key `<key>`."

- [ ] **Step 3: Build + manual verify:** Add photo → pick a PNG → preview shows it immediately, browser opens the portal Art Assets page, Explorer selects the normalized 1024² file. Capture evidence.

- [ ] **Step 4: Commit**

```bash
git add app/include/AppController.h app/src/AppController.cpp app/qml/RulesPage.qml app/qml/AssetManager.qml
git commit -m "Add photo: local save + normalize + open portal for one-drop upload"
```

---

### Task 10: Preview fidelity (PreviewPage.qml)

**Files:**
- Modify: `app/qml/PreviewPage.qml:42-110`

- [ ] **Step 1:** Remove the `"PLAYING A GAME"` label and the 🎮/💻 emoji blocks.

- [ ] **Step 2:** Big image → `Image { source: AppController.presenceLargeImageSource; fillMode: Image.PreserveAspectCrop }` with a neutral placeholder `Rectangle` shown only when `source === "" || status !== Image.Ready`. Small overlay → `presenceSmallImageSource` similarly.

- [ ] **Step 3:** Keep the name/details/state `Text` bindings (already correct: `presenceName/Details/State`) — they now read the same payload the client publishes (Task 4/5), so the preview matches Discord.

- [ ] **Step 4: Build + manual verify:** with RuneLite feeding context, the preview shows the real art + "RuneLight – Training Crafting" + "Grand Exchange", matching Discord (no more "Old School RuneScape" / 🎮). Capture before/after screenshots.

- [ ] **Step 5: Commit**

```bash
git add app/qml/PreviewPage.qml
git commit -m "Preview mirrors the published payload (real art, name, state)"
```

---

### Task 11: Committed placeholder-art generator (replace lost Pillow script)

**Files:**
- Create: `scripts/make-placeholder-art.cpp` (+ a `scripts/CMakeLists.txt` target `make-placeholder-art`, optional)

- [ ] **Step 1:** Write a tiny `QGuiApplication`-free `QImage` program that draws a 1024×1024 cyan-on-`#1e1f22` rounded square with a 2–4 char uppercase monogram (args: `<out.png> <monogram>`), mirroring the existing `code.png`/`osrs.png` style. Reuse `ArtStore`'s normalize indirectly (it draws, then could pass through importImage).

- [ ] **Step 2:** Generate the missing keys' placeholders into `app/resources/assets/` (`terminal`, `youtube`, `reddit`, `pihole`, `dashboard`, `discord`, `computer`) so the Asset Manager + preview light up.

- [ ] **Step 3:** Add a test asserting output is a 1024×1024 PNG (extend test_art_store).

- [ ] **Step 4: Commit**

```bash
git add scripts/make-placeholder-art.cpp scripts/CMakeLists.txt app/resources/assets/*.png
git commit -m "Add committed placeholder-art generator + remaining art keys"
```

---

## Self-Review

**Spec coverage:**
- A (backend wiring) → Tasks 2,3,5,6. B (main line) → Task 4. C (preview) → Task 10. D (capture→rule) → Task 8. E (simplified form) → Task 7. F (add photo) → Tasks 2,9. G (terminal title) → Task 1. "How images made" gap → Task 11. ✅ all covered.

**Placeholder scan:** No "TBD"/"handle errors" hand-waves; error paths shown (importImage err, addPhoto failure). Two spots say "confirm the exact signature in RuleEngine.h" (Task 4) and reference the existing friendly-name map (Task 8) — these are reads of existing code the implementer must do, not unwritten logic.

**Type consistency:** `largeImageKey`, `activityNameTemplate`, `stateTemplate`, `privacyLevel`, `StatusDisplay::Name` match `Rule.h`/`PresencePayload.h`. `slugify`/`importImage`/`localPathForKey` signatures consistent across Tasks 2/9. `availableContextFields` returns `{token,label}` used identically in Task 7. `seedRuleFromCapture`/`addRule`/`updateRuleField`/`saveRules`/`rulesList`/`ruleAt` names consistent across Tasks 6/8/9.

**Note for executor:** Tasks 1–4,11 have real unit tests (TDD). Tasks 5–10 are QML/integration — verified by building on Windows and capturing PrintWindow screenshots + checking `runlog.txt`, since QML UI can't be unit-tested here. Build loop: `rebuild-inc.bat` (incremental) or `build-discord.bat` then `windeployqt` (clean).

# Rule-editor & presence polish — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make presence rules nameable up-front, clarify the Public/Private toggle, add an in-app monogram art generator, render the terminal coding line as "Coding – {tab}", document the portal art-upload step, and resolve the "RuneLight" Discord game-detection question.

**Architecture:** Targeted edits to the existing simplified editor (`app/qml/RulesPage.qml`), the art plumbing (`ArtStore`, `AppController`), one rule template (`config/omnipresence.example.json` + live `%APPDATA%` config), plus one research task. Logic changes are TDD in the existing QtTest harness under `tests/`; QML/copy/doc changes are verified by click-through on the Windows `build-discord` build.

**Tech Stack:** C++20, Qt 6.8.3 (Quick/QML, Gui, Test), CMake/Ninja, Discord Social SDK (`discord_partner_sdk`). Build on MasterRig's Windows 11 host via WSL interop.

## Global Constraints

- Target runtime: **Windows 11**. App is built on Windows (`C:\dev\OmniPresence`, `build-discord/`), not WSL.
- Live behaviour = compiled exe **+** `%APPDATA%\OmniPresence\config.json` (two synced copies: `Roaming\OmniPresence\` and `Roaming\OmniPresence\OmniPresence\`). Editing `config/omnipresence.example.json` only affects **fresh installs**. To change a live rule, edit the `%APPDATA%` config **with the app stopped** (it overwrites on save).
- **Do NOT add any VS Code rule, match criterion, or icon.** The user uses **Windows Terminal**, not VS Code. (Existing `vscode.*` template fallbacks already in the config may stay; just don't add new ones.)
- Art keys are lowercase `[a-z0-9_]` slugs (`ArtStore::slugify`).
- Discord falls back to the **app icon** for any `largeImageKey` not uploaded to Portal → Rich Presence → Art Assets. There is no API to upload assets; upload is a manual one-drag step.
- Monogram art = 1024×1024 PNG, Discord-dark `#1e1f22` bg, rounded accent panel, default accent cyan `#22d3ee`.
- After any clean rebuild re-run `windeployqt --qmldir app\qml build-discord\app\omnipresence.exe`; after a new QML import the same; incremental relinks keep the Qt runtime.
- After code changes run `graphify update .` (AST-only, no API cost).
- Commit messages end with the two standard trailers (Co-Authored-By + Claude-Session).

---

### Task 1: DRY-refactor monogram rendering into `ArtStore::renderMonogram`

Move the pixel-drawing core out of the standalone `make-placeholder-art` tool into a reusable `ArtStore` static, so the app can generate the same tiles. The CLI keeps working by calling the new static.

**Files:**
- Modify: `app/include/ArtStore.h`
- Modify: `app/src/ArtStore.cpp`
- Modify: `scripts/make-placeholder-art.cpp` (becomes a thin CLI over the static)
- Modify: `scripts/CMakeLists.txt` (compile `ArtStore.cpp` into the tool)
- Test: `tests/test_art_store.cpp`

**Interfaces:**
- Produces: `static bool OmniPresence::ArtStore::renderMonogram(const QString& outPath, const QString& monogram, const QColor& accent, QString* err)` — writes a 1024² PNG to `outPath`, creating parent dirs; returns `false` and sets `*err` on write failure.

- [ ] **Step 1: Write the failing test**

Add to `tests/test_art_store.cpp` (new private slot, after `importRejectsUnreadable`):

```cpp
    void renderMonogramWrites1024Png() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString out = tmp.filePath(QStringLiteral("sub/mono.png"));  // nested → tests mkpath
        QString err;
        QVERIFY2(ArtStore::renderMonogram(out, QStringLiteral("YT"),
                 QColor(QStringLiteral("#ff4444")), &err), qPrintable(err));
        const QImage got(out);
        QVERIFY(!got.isNull());
        QCOMPARE(got.width(),  1024);
        QCOMPARE(got.height(), 1024);
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run (Windows): `tests via testbuild.bat` (see Build notes). Expected: compile error — `renderMonogram` is not a member of `ArtStore`.

- [ ] **Step 3: Add the declaration**

In `app/include/ArtStore.h`, add inside `class ArtStore { public: … }` after `slugify`:

```cpp
    /// Render a 1024x1024 monogram tile (Discord-dark bg + accent panel) to outPath.
    /// Creates parent dirs. Returns false (and sets *err) on write failure.
    static bool renderMonogram(const QString& outPath, const QString& monogram,
                               const QColor& accent, QString* err);
```

Add the needed include at the top of `ArtStore.h`:

```cpp
#include <QColor>
```

- [ ] **Step 4: Implement the static**

In `app/src/ArtStore.cpp`, add these includes near the existing ones:

```cpp
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QFont>
#include <QFileInfo>
```

Add the implementation (inside `namespace OmniPresence`, after `importImage`):

```cpp
bool ArtStore::renderMonogram(const QString& outPath, const QString& monogram,
                              const QColor& accent, QString* err) {
    const int S = 1024;
    QImage img(S, S, QImage::Format_ARGB32);
    img.fill(QColor(QStringLiteral("#1e1f22")));        // Discord dark

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    QPainterPath panel;
    panel.addRoundedRect(96, 96, S - 192, S - 192, 96, 96);
    p.fillPath(panel, QColor(accent.red(), accent.green(), accent.blue(), 38));
    p.setPen(QPen(accent, 10));
    p.drawPath(panel);

    QFont f;
    f.setBold(true);
    f.setPixelSize(monogram.length() <= 2 ? 460 : 300);
    p.setFont(f);
    p.setPen(accent);
    p.drawText(QRect(0, 0, S, S), Qt::AlignCenter, monogram);
    p.end();

    const QString parent = QFileInfo(outPath).absolutePath();
    if (!parent.isEmpty()) QDir().mkpath(parent);
    if (!img.save(outPath, "PNG")) {
        if (err) *err = QStringLiteral("Cannot write %1").arg(outPath);
        return false;
    }
    return true;
}
```

- [ ] **Step 5: Point the CLI at the static**

Replace the body of `scripts/make-placeholder-art.cpp` with:

```cpp
// make-placeholder-art.cpp — thin CLI over ArtStore::renderMonogram.
//   make-placeholder-art terminal.png ">_"
//   make-placeholder-art youtube.png "YT" "#ff4444"
#include <QGuiApplication>
#include <QColor>
#include "ArtStore.h"
#include <cstdio>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);   // needed for font rendering
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <out.png> <MONOGRAM> [#accentHex]\n", argv[0]);
        return 2;
    }
    const QString outPath  = QString::fromLocal8Bit(argv[1]);
    const QString monogram = QString::fromLocal8Bit(argv[2]);
    const QColor  accent   = (argc >= 4) ? QColor(QString::fromLocal8Bit(argv[3]))
                                         : QColor(QStringLiteral("#22d3ee"));
    QString err;
    if (!OmniPresence::ArtStore::renderMonogram(outPath, monogram, accent, &err)) {
        std::fprintf(stderr, "%s\n", qPrintable(err));
        return 1;
    }
    std::printf("wrote %s (1024x1024)\n", qPrintable(outPath));
    return 0;
}
```

- [ ] **Step 6: Wire ArtStore into the tool build**

Replace `scripts/CMakeLists.txt` with:

```cmake
# scripts/CMakeLists.txt — dev tooling, not part of the app.
find_package(Qt6 REQUIRED COMPONENTS Gui)

add_executable(make-placeholder-art
    make-placeholder-art.cpp
    ${CMAKE_SOURCE_DIR}/app/src/ArtStore.cpp)
target_include_directories(make-placeholder-art PRIVATE ${CMAKE_SOURCE_DIR}/app/include)
target_link_libraries(make-placeholder-art PRIVATE Qt6::Gui)
```

- [ ] **Step 7: Run test to verify it passes**

Run (Windows): build `test_art_store` and run it. Expected: PASS, all 4 slots green.
Note: `test_art_store` already creates a `QGuiApplication` (QTEST_MAIN with Qt6::Gui linked), so font rendering works. If it fails to start on a headless build, set `QT_QPA_PLATFORM=offscreen` for the test run.

- [ ] **Step 8: Commit**

```bash
git add app/include/ArtStore.h app/src/ArtStore.cpp scripts/make-placeholder-art.cpp scripts/CMakeLists.txt tests/test_art_store.cpp
git commit -m "Extract ArtStore::renderMonogram (DRY w/ make-placeholder-art)"
```

---

### Task 2: `AppController::generateArt` + shared `finishArtImport`

Add the controller bridge that generates a tile for a rule and runs the same persist + portal hand-off as Add-photo. Extract the shared tail of `importPhoto` into a helper.

**Files:**
- Modify: `app/include/AppController.h`
- Modify: `app/src/AppController.cpp`

**Interfaces:**
- Consumes: `ArtStore::renderMonogram` (Task 1), existing `ArtStore::slugify`, `ArtStore::artDir()`, `updateRuleField`, `m_configStore->setAssetKey`, `saveRules`.
- Produces: `Q_INVOKABLE QString AppController::generateArt(int ruleIndex, const QString& monogram, const QString& accentHex)` — renders `<artDir>/<slug(ruleName)>.png`, sets the rule's `largeImageKey`, persists, opens the portal + reveals the file. Returns the key ("" on failure).

- [ ] **Step 1: Declare the methods**

In `app/include/AppController.h`, in the "Rule CRUD bridge (QML)" block after `importPhoto`:

```cpp
    /// Generate a monogram tile for the rule (key = slug of its name), set it as
    /// the rule's art, persist, and open the portal for upload. "" on failure.
    Q_INVOKABLE QString      generateArt(int ruleIndex, const QString& monogram, const QString& accentHex);
```

In the `private:` section after `QString sourceForKey(...) const;`:

```cpp
    /// Persist a freshly-stored art key onto a rule, then open portal + reveal file.
    QString finishArtImport(int ruleIndex, const QString& key, const QString& outPath);
```

- [ ] **Step 2: Add the helper + generateArt**

In `app/src/AppController.cpp`, add an include if missing (top): `#include <QColor>`.

Add `finishArtImport` and `generateArt` just before the closing `} // namespace OmniPresence` (after `importPhoto`):

```cpp
QString AppController::finishArtImport(int ruleIndex, const QString& key, const QString& outPath) {
    updateRuleField(ruleIndex, QStringLiteral("largeImageKey"), key);
    m_configStore->setAssetKey(key, key);
    saveRules();

    const QString appId = m_configStore->settings().discordAppId;
    if (!appId.isEmpty()) {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://discord.com/developers/applications/%1/rich-presence/assets").arg(appId)));
    }
    QProcess::startDetached(QStringLiteral("explorer.exe"),
        {QStringLiteral("/select,"), QDir::toNativeSeparators(outPath)});
    return key;
}

QString AppController::generateArt(int ruleIndex, const QString& monogram, const QString& accentHex) {
    const QList<Rule>& rules = m_configStore->ruleSet().rules();
    if (ruleIndex < 0 || ruleIndex >= rules.size()) return {};

    const QString name = rules[ruleIndex].name;
    const QString key  = ArtStore::slugify(name.isEmpty() ? QStringLiteral("art") : name);
    const QColor  accent = accentHex.isEmpty() ? QColor(QStringLiteral("#22d3ee"))
                                               : QColor(accentHex);
    const QString mono = monogram.isEmpty() ? name.left(2).toUpper() : monogram;

    QDir().mkpath(m_artStore.artDir());
    const QString out = QDir(m_artStore.artDir()).filePath(key + QStringLiteral(".png"));

    QString err;
    if (!ArtStore::renderMonogram(out, mono, accent, &err)) {
        m_discordError = err;
        emit discordStatusChanged();
        return {};
    }
    return finishArtImport(ruleIndex, key, out);
}
```

- [ ] **Step 3: Refactor `importPhoto` to reuse the helper**

In `app/src/AppController.cpp`, replace the tail of `importPhoto` (from the `updateRuleField(... "largeImageKey" ...)` line through the `explorer.exe` block and `return key;`) with:

```cpp
    return finishArtImport(ruleIndex, key, out);
```

So the end of `importPhoto` reads:

```cpp
    QString out, err;
    if (!m_artStore.importImage(src, key, &out, &err)) {
        m_discordError = err;
        emit discordStatusChanged();
        return {};
    }
    return finishArtImport(ruleIndex, key, out);
}
```

- [ ] **Step 4: Verify it compiles**

Run (Windows): incremental app build (`rebuild-inc.bat` or `cmake --build build-discord`). Expected: links clean, `omnipresence.exe` produced. (No unit test — `generateArt` touches `QDesktopServices`/`QProcess`; its logic is `slugify` (already tested) + `renderMonogram` (Task 1). Behaviour is verified live in Task 8.)

- [ ] **Step 5: Commit**

```bash
git add app/include/AppController.h app/src/AppController.cpp
git commit -m "Add AppController::generateArt + shared finishArtImport"
```

---

### Task 3: Surface the rule name + clarify Public/Private wording

UI-only edits to the simplified editor: a "Rule name" field at the top, removal of the duplicate from Advanced, and clearer toggle copy.

**Files:**
- Modify: `app/qml/RulesPage.qml`

**Interfaces:**
- Consumes: existing `root.setField("name", …)`, `root.current.name`, `root.current.privacyLevel`.

- [ ] **Step 1: Add the "Rule name" field at the top of the editor**

In `app/qml/RulesPage.qml`, immediately after `Text { text: "Edit Rule"; … }` (line ~151) and **before** the `// 1 — Main line` block, insert:

```qml
                // 0 — Rule name (what you call this rule)
                Label2 { text: "Rule name" }
                TextField {
                    Layout.fillWidth: true
                    text: root.current.name || ""
                    placeholderText: "e.g. RuneScape, Coding, YouTube"
                    onTextEdited: root.setField("name", text)
                    color: "#dbdee1"
                    background: Rectangle { radius: 4; color: "#1e1f22" }
                }
```

- [ ] **Step 2: Remove the duplicate name row from Advanced**

In the `Advanced` `ColumnLayout`, delete this line:

```qml
                    AdvRow { label: "Name (internal)";   value: root.current.name || "";                 onCommit: root.setField("name", v) }
```

- [ ] **Step 3: Reword the Public/Private toggle text**

Replace the toggle `Text { … }` (the one bound to `privacyLevel`, lines ~171-177) with:

```qml
                    Text {
                        text: ((root.current.privacyLevel || 0) === 2)
                              ? "Private — this window will not show its details"
                              : "Public — this window shows its details"
                        color: "#dbdee1"; font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
```

- [ ] **Step 4: Verify (build + click-through)**

Deploy to `build-discord` (Task 8 build steps) and open Rules. Expected: "Rule name" is the first field and renaming updates the left list live; the Name row is gone from Advanced; toggling Public/Private flips the sentence as worded above. No QML console errors.

- [ ] **Step 5: Commit**

```bash
git add app/qml/RulesPage.qml
git commit -m "Surface rule name in editor + clearer Public/Private wording"
```

---

### Task 4: In-app "Generate" button + monogram popup

Add a Generate button beside "Add photo…" that opens a small popup (monogram + colour swatches) and calls `generateArt`.

**Files:**
- Modify: `app/qml/RulesPage.qml`

**Interfaces:**
- Consumes: `AppController.generateArt(int, string, string)` (Task 2), existing `root.loadCurrent()`, `uploadHint`.

- [ ] **Step 1: Add the Generate button**

In the Image `RowLayout` (after the "Add photo…" `Button`, before the row closes), insert:

```qml
                    Button {
                        text: "Generate"
                        onClicked: genPopup.open()
                        background: Rectangle { radius: 6; color: parent.hovered ? "#3a3d44" : "#2b2d31"; border.color: "#5865f2"; border.width: 1 }
                        contentItem: Text { text: parent.text; color: "#dbdee1"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        implicitHeight: 34; implicitWidth: 96
                    }
```

- [ ] **Step 2: Add the popup**

Inside `Page { id: root … }`, after the `FileDialog { id: photoDialog … }` block, add:

```qml
    Popup {
        id: genPopup
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 320
        padding: 16
        property string accent: "#22d3ee"
        background: Rectangle { radius: 8; color: "#2b2d31"; border.color: "#1e1f22" }

        onOpened: monoField.text = (root.current.name || "").substring(0, 2).toUpperCase()

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text { text: "Generate image"; color: "#dbdee1"; font.pixelSize: 15; font.bold: true }

            Text { text: "Monogram"; color: "#949ba4"; font.pixelSize: 11 }
            TextField {
                id: monoField
                Layout.fillWidth: true
                maximumLength: 3
                color: "#dbdee1"
                background: Rectangle { radius: 4; color: "#1e1f22" }
            }

            Text { text: "Colour"; color: "#949ba4"; font.pixelSize: 11 }
            RowLayout {
                spacing: 8
                Repeater {
                    model: ["#22d3ee", "#ff4444", "#23a55a", "#faa81a", "#5865f2", "#eb459e"]
                    delegate: Rectangle {
                        required property string modelData
                        width: 32; height: 32; radius: 6; color: modelData
                        border.width: genPopup.accent === modelData ? 3 : 0
                        border.color: "#ffffff"
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: genPopup.accent = parent.modelData }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Item { Layout.fillWidth: true }
                Button {
                    text: "Cancel"
                    onClicked: genPopup.close()
                    background: Item {}
                    contentItem: Text { text: parent.text; color: "#949ba4"; horizontalAlignment: Text.AlignHCenter }
                }
                Button {
                    text: "Create"
                    onClicked: {
                        var key = AppController.generateArt(root.selectedIndex, monoField.text, genPopup.accent)
                        if (key !== "") {
                            root.loadCurrent()
                            uploadHint.text = "Generated \"" + key + "\". Drop the file Explorer just "
                                + "revealed into the Art Assets page that opened, then Save."
                            uploadHint.visible = true
                        }
                        genPopup.close()
                    }
                    background: Rectangle { radius: 6; color: parent.hovered ? "#4752c4" : "#5865f2" }
                    contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    implicitWidth: 90; implicitHeight: 32
                }
            }
        }
    }
```

(Requires `import QtQuick.Controls` and `QtQuick.Layouts`, both already imported. `Overlay` comes from `QtQuick.Controls`.)

- [ ] **Step 3: Verify (build + click-through)**

Deploy, open a rule, click Generate. Expected: popup pre-fills the monogram from the rule name, a swatch is selectable, Create makes the preview light up immediately, Explorer reveals `…\art\<key>.png`, and the portal Art Assets page opens. No QML console errors.

- [ ] **Step 4: Commit**

```bash
git add app/qml/RulesPage.qml
git commit -m "Add in-app Generate monogram art button + popup"
```

---

### Task 5: Terminal coding main line → "Coding – {tab}"

Change the Windows-Terminal rule's main line to "Coding – {{terminal.title or window.title}}". TDD the render, then update the example config and the live config.

**Files:**
- Test: `tests/test_rule_engine_render.cpp`
- Modify: `config/omnipresence.example.json`
- Modify (live, app stopped): `%APPDATA%\OmniPresence\config.json` (both copies)

**Interfaces:**
- Consumes: existing `RuleEngine::evaluate`, `{{a or b}}` template + dangling-separator trim.

- [ ] **Step 1: Write the failing tests**

Add to `tests/test_rule_engine_render.cpp` a rule builder (after `runeLightRule()`):

```cpp
static Rule terminalRule() {
    Rule r;
    r.id                     = QStringLiteral("term");
    r.name                   = QStringLiteral("Coding");
    r.enabled                = true;
    r.priority               = 20;
    r.matchProcessName       = QStringLiteral("WindowsTerminal.exe");
    r.matchIntegrationSource = QStringLiteral("terminal");
    r.activityType           = ActivityType::Playing;
    r.activityNameTemplate   = QStringLiteral("Coding – {{terminal.title or window.title}}");
    r.largeImageKey          = QStringLiteral("code");
    r.privacyLevel           = PrivacyLevel::Public;
    return r;
}
```

Add two slots inside `TestRuleEngineRender`:

```cpp
    void terminalMainLineUsesTabTitle() {
        RuleSet rules; rules.addRule(terminalRule());
        IntegrationContext integ;
        integ.update(QStringLiteral("terminal"), QJsonObject{
            {QStringLiteral("repo"), QString()},
        });
        WindowInfo win;
        win.processName = QStringLiteral("windowsterminal.exe");
        win.windowTitle = QStringLiteral("RAM");
        RuleEngine engine; ManualOverrideState ov; PresencePayload prev;
        const PresencePayload p = engine.evaluate(win, integ, rules, ov, prev);
        QCOMPARE(p.name, QStringLiteral("Coding – RAM"));
    }

    void terminalNoTitleDropsSeparator() {
        RuleSet rules; rules.addRule(terminalRule());
        IntegrationContext integ;
        integ.update(QStringLiteral("terminal"), QJsonObject{
            {QStringLiteral("repo"), QString()},
        });
        WindowInfo win;
        win.processName = QStringLiteral("windowsterminal.exe");   // no windowTitle
        RuleEngine engine; ManualOverrideState ov; PresencePayload prev;
        const PresencePayload p = engine.evaluate(win, integ, rules, ov, prev);
        QCOMPARE(p.name, QStringLiteral("Coding"));
    }
```

- [ ] **Step 2: Run tests to verify they fail**

Run (Windows): build + run `test_rule_engine_render`. Expected: the two new cases FAIL (until the template renders the combined line / trims correctly). If both pass immediately, the engine already supports it — proceed; the real deliverable is the config change.

- [ ] **Step 3: Update the example config (fresh installs)**

In `config/omnipresence.example.json`, the WindowsTerminal rule (the `matchProcessName: "WindowsTerminal.exe"` object), change:

```json
      "activityNameTemplate": "Code",
```

to:

```json
      "activityNameTemplate": "Coding – {{terminal.title or window.title}}",
```

Leave `detailsTemplate` / `stateTemplate` as-is.

- [ ] **Step 4: Run tests to verify they pass**

Run (Windows): `test_rule_engine_render`. Expected: PASS (all cases, including the two new ones).

- [ ] **Step 5: Patch the live config (so the running app reflects it)**

**App must be stopped first** (`taskkill /IM omnipresence.exe /F`). In **both** live copies
`C:\Users\grafe.MASTERRIG\AppData\Roaming\OmniPresence\config.json` and
`…\OmniPresence\OmniPresence\config.json`, find the WindowsTerminal rule and set its
`activityNameTemplate` to `"Coding – {{terminal.title or window.title}}"`.

Do this with a small script (run from WSL; edits the Windows file in place):

```bash
python3 - <<'EOF'
import json
for p in [
    "/mnt/c/Users/grafe.MASTERRIG/AppData/Roaming/OmniPresence/config.json",
    "/mnt/c/Users/grafe.MASTERRIG/AppData/Roaming/OmniPresence/OmniPresence/config.json",
]:
    try:
        d = json.load(open(p, encoding="utf-8"))
    except FileNotFoundError:
        print("skip (absent):", p); continue
    changed = False
    for r in d.get("rules", []):
        if r.get("matchProcessName") == "WindowsTerminal.exe":
            r["activityNameTemplate"] = "Coding – {{terminal.title or window.title}}"
            changed = True
    json.dump(d, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    print(("updated" if changed else "no-terminal-rule"), p)
EOF
```

(If the live config has **no** WindowsTerminal rule — e.g. terminal sessions are hitting the generic fallback, which would explain the bare "code" — add one mirroring the example-config rule. The script prints `no-terminal-rule` in that case; in the same run append a copy of the example WindowsTerminal rule object to `d["rules"]`.)

- [ ] **Step 6: Commit**

```bash
git add tests/test_rule_engine_render.cpp config/omnipresence.example.json
git commit -m "Terminal coding main line -> 'Coding – {tab}'"
```

---

### Task 6: Document the portal art-upload requirement

The "VS Code shows the OmniPresence logo" symptom is the unknown-key → app-icon fallback. Make the requirement explicit so future-us doesn't re-debug it as a code bug.

**Files:**
- Modify: `docs/DISCORD_SETUP.md` (or the doc that covers Rich Presence assets — confirm by listing `docs/`; if a dedicated assets section exists elsewhere, add there instead).

**Interfaces:** none (docs only).

- [ ] **Step 1: Add the section**

Append to `docs/DISCORD_SETUP.md` a section:

```markdown
## Rich Presence art assets (why your icon shows the app logo)

Discord only renders a presence image if its `largeImageKey` / `smallImageKey`
exists under **Developer Portal → your app → Rich Presence → Art Assets**. For any
key it doesn't recognise, Discord silently falls back to the **application icon**
(the OmniPresence "OP" logo). This is not an app bug.

OmniPresence stores generated/imported tiles locally (`%APPDATA%\OmniPresence\art\<key>.png`)
so the in-app preview is correct immediately, but **there is no API to upload art
assets** — it's a manual step. When you Generate or Add-photo, the app reveals the
PNG in Explorer and opens the Art Assets page; drag the file in, name the asset
**exactly** the key shown in the upload hint (e.g. `code`, `youtube`), and Save.
Allow a minute for Discord to propagate before the icon appears on your presence.
```

- [ ] **Step 2: Commit**

```bash
git add docs/DISCORD_SETUP.md
git commit -m "Document portal art-asset upload requirement"
```

---

### Task 7: RuneLight — can the SDK override Discord's game detection? (research)

Determine whether our Social-SDK presence can suppress/override Discord's own detected-game label ("RuneLight"). Implement if cheap+possible; otherwise document the client-side setting.

**Files:**
- Modify: `docs/DECISIONS.md` (record the finding)
- Possibly modify: `app/src/DiscordPresenceClient.cpp` / `app/include/PresencePayload.h` (only if an override exists)

**Interfaces:** TBD by research outcome — if an SDK flag exists, expose it; otherwise none.

- [ ] **Step 1: Investigate the SDK surface**

Read `third_party/discord_social_sdk/include/discordpp.h` for anything about
suppressing/overriding detected games or activity precedence. Search terms:

```bash
grep -inE "detect|registered|override|suppress|gameActivity|verified|game" third_party/discord_social_sdk/include/discordpp.h | head -40
```

Also re-check memory `reference_discord_social_sdk_cpp` and the official Social SDK
Rich Presence docs (WebFetch the Discord developer docs page for Rich Presence /
"How Discord detects games" / Activity Privacy). Capture: does Discord show **both** a
detected game and an SDK Rich Presence, and which wins on the profile?

- [ ] **Step 2: Decide and act**

- **If an SDK-side override/precedence flag exists:** implement the minimal change in
  `DiscordPresenceClient` (and a `PresencePayload` field if needed), rebuild, and
  verify on Discord that only OmniPresence's line shows. Add a focused note here.
- **If not (most likely):** the fix is client-side — Discord → **Settings → Registered
  Games** (remove/disable RuneLight) and/or **Activity Privacy → "Display current
  activity as a status message"** controls. Document the exact path that makes only
  OmniPresence's presence show.

- [ ] **Step 3: Record the decision**

Append to `docs/DECISIONS.md`:

```markdown
## RuneLight game-detection vs our SDK presence (2026-06-21)

"RuneLight" on the profile comes from Discord's own game detection (registered-game
database), independent of our Social-SDK Rich Presence. Finding: <SDK override
possible? yes/no + how>. Resolution: <implemented flag X | client-side setting:
Settings → Registered Games / Activity Privacy → …>. With that applied, the
profile shows OmniPresence's published name/line.
```

- [ ] **Step 4: Commit**

```bash
git add docs/DECISIONS.md
# include DiscordPresenceClient.cpp / PresencePayload.h only if changed
git commit -m "Resolve RuneLight game-detection question (research + decision)"
```

---

### Task 8: Build, deploy, verify on Windows, sync vault

Bring it all together on the live Windows build and confirm the user-visible behaviours.

**Files:**
- Modify: `~/notes/vault/Projects/OmniPresence.md` (status update)

- [ ] **Step 1: Sync source to the Windows build dir + build**

```bash
/mnt/c/Windows/System32/taskkill.exe /IM omnipresence.exe /F >/dev/null 2>&1; sleep 1
rsync -a --exclude='build-discord/' --exclude='build/' --exclude='.git/' \
  --exclude='third_party/' --exclude='graphify-out/' \
  /home/grafe/code/OmniPresence/ /mnt/c/dev/OmniPresence/
```

Then build on Windows (`build-discord`, incremental to keep the Qt runtime):

```
cmd /c "cd C:\dev\OmniPresence && rebuild-inc.bat"
```

- [ ] **Step 2: Re-deploy the new QML import if needed**

`RulesPage.qml` already imports `QtQuick.Dialogs` (from the prior pass) and now uses
`Popup`/`Overlay` from `QtQuick.Controls` (already deployed). No **new** module is
introduced, so a `windeployqt` re-run is only needed after a *clean* rebuild. If you did
a clean rebuild:

```
cmd /c "cd C:\dev\OmniPresence && windeployqt --qmldir app\qml build-discord\app\omnipresence.exe"
```

- [ ] **Step 3: Run the unit tests**

```
cmd /c "cd C:\dev\OmniPresence && testbuild.bat"
```

Expected: `test_template_engine`, `test_art_store` (now incl. `renderMonogramWrites1024Png`),
`test_config_assets`, `test_rule_engine_render` (now incl. the two terminal cases) all PASS.

- [ ] **Step 4: Launch + click-through verify (non-elevated)**

Launch `omnipresence.exe` non-elevated; approve the Discord prompt if needed. Verify:
1. Rules: "Rule name" is the first field, renames update the list live; no Name row in Advanced.
2. Public/Private toggle text reads "…shows its details" / "…will not show its details".
3. Generate → popup → Create lights up the preview, reveals the PNG, opens the portal.
4. A Windows-Terminal tab titled e.g. "RAM" publishes main line **"Coding – RAM"** (check `runlog.txt`).
5. After uploading the coding tile to the portal, the coding icon shows on the presence (allow propagation).

- [ ] **Step 5: graphify + vault**

```bash
cd /home/grafe/code/OmniPresence && graphify update .
```

Patch `~/notes/vault/Projects/OmniPresence.md`: add a dated "Rule-editor & presence polish — shipped" section summarising the six changes, the RuneLight finding, and any new gotcha; tick/О update the relevant Open todos (art-asset upload for the coding tile, terminal line).

- [ ] **Step 6: Final commit + worklog**

```bash
git add -A && git commit -m "Build/deploy verify: rule-editor & presence polish"
worklog add -p "OmniPresence" -c personal -s done -e 120 --auto-tokens \
  "Rule-editor & presence polish shipped" \
  "Rule name surfaced, clearer privacy copy, in-app monogram generator, 'Coding – {tab}', art-upload doc, RuneLight detection resolved"
```

---

## Self-Review

**Spec coverage:**
1. Surface rule name → Task 3 ✓
2. Public/Private wording → Task 3 ✓
3. In-app monogram generator → Task 1 (render core) + Task 2 (bridge) + Task 4 (UI) ✓
4. Terminal "Coding – {tab}" → Task 5 ✓
5. Art-on-Discord / portal upload doc → Task 6 (+ generate/import flow in 2/4) ✓
6. RuneLight override research → Task 7 ✓
- VS Code dropped → Global Constraints + Task 5 note ✓

**Placeholder scan:** Task 7 deliberately leaves the *outcome* open (research), but every step states exactly what to read, the decision branches, and the doc to write — no "TBD" in actionable steps. All code steps show complete code.

**Type consistency:** `renderMonogram(QString,QString,QColor,QString*)→bool` defined Task 1, consumed Task 2 + CLI ✓. `generateArt(int,QString,QString)→QString` defined Task 2, consumed Task 4 QML ✓. `finishArtImport(int,QString,QString)→QString` defined + used Task 2 ✓. Art key = `slugify(name)` consistent across Task 2 + verify. Template `"Coding – {{terminal.title or window.title}}"` identical in Task 5 test + example config + live patch ✓.

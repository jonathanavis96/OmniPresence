// RulesPage.qml — Simplified presence-rule editor.
//
// Visible fields: Main line, Public/Private, optional extra detail, image.
// Everything else (priority, regex, raw match criteria, raw templates, timestamp,
// small image) lives under a collapsed "Advanced" section. Rules are read/written
// through AppController's real CRUD bridge.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Page {
    id: root
    readonly property string pageId: "RulesPage.qml"
    background: Rectangle { color: "#313338" }

    // Pre-select a rule (e.g. the one just seeded from a captured window).
    property int startIndex: -1
    property int selectedIndex: -1
    property var ruleItems: []
    property var current: ({})
    property bool advancedOpen: false

    function refreshList() {
        ruleItems = AppController.rulesList()
    }
    function loadCurrent() {
        current = (selectedIndex >= 0) ? AppController.ruleAt(selectedIndex) : ({})
        syncBuilder()
    }

    // ── Source-aware Main-line builder ────────────────────────────────────────
    // A rule belongs to one source (RuneScape / browser / generic app). The token
    // dropdown only offers tokens that make sense for that source — RuneScape
    // tokens never clutter a browser or terminal rule, and vice-versa.
    function ruleSource(c) {
        c = c || ({})
        var src  = (c.matchIntegrationSource || "").toLowerCase()
        var tmpl = ((c.activityNameTemplate || "") + (c.stateTemplate || "")).toLowerCase()
        if (src.indexOf("runelite") >= 0 || src.indexOf("osrs") >= 0 || tmpl.indexOf("runelite.") >= 0) return "runelite"
        if ((c.matchBrowserDomain || "") !== "" || src.indexOf("browser") >= 0 || tmpl.indexOf("browser.") >= 0) return "browser"
        return "app"
    }
    function tokenModel(c) {
        var runescape = [
            { label: "RuneScape activity",     token: "{{runelite.activity}}" },
            { label: "RuneScape target / NPC", token: "{{runelite.target}}" },
            { label: "RuneScape skill",        token: "{{runelite.skill}}" },
            { label: "RuneScape location",     token: "{{runelite.location}}" }
        ]
        var browser = [
            { label: "Show name from URL", token: "{{browser.label}}" },
            { label: "Page / video title", token: "{{browser.title}}" },
            { label: "Site name",          token: "{{browser.site}}" }
        ]
        var winTitle = { label: "Window / tab title",       token: "{{window.title}}" }
        var docTitle = { label: "Document / tab name only", token: "{{window.doctitle}}" }
        var urlLabel = { label: "Show name from URL",       token: "{{browser.label}}" }
        var nothing  = { label: "Nothing extra",            token: "" }
        var src = ruleSource(c)
        if (src === "runelite") return runescape.concat([winTitle, nothing])
        if (src === "browser")  return browser.concat([winTitle, nothing])
        // Generic app (e.g. Windows Terminal). "Show name from URL" stays available
        // because it's harmless and occasionally useful everywhere.
        return [winTitle, docTitle, urlLabel, nothing]
    }
    // Reflect the rule's REAL template back into the two dropdowns, so the builder
    // never lies (it used to always read "Watching / Show name from URL"). Splits
    // "Verb {{token}}" into the verb box + the matching token entry.
    function syncBuilder() {
        if (typeof verbCombo === "undefined" || typeof fillCombo === "undefined") return
        var t = root.current.activityNameTemplate || ""
        var m = t.match(/\{\{[^}]+\}\}/)
        var token = m ? m[0] : ""
        var verb  = (token ? t.replace(token, "") : t).trim()
        verbCombo.editText = verb
        var idx = fillCombo.count - 1 // default → "Nothing extra"
        for (var i = 0; i < fillCombo.count; i++) {
            if (fillCombo.model[i] && fillCombo.model[i].token === token) { idx = i; break }
        }
        fillCombo.currentIndex = idx
    }
    function setField(field, value) {
        if (selectedIndex >= 0) AppController.updateRuleField(selectedIndex, field, value)
    }

    Component.onCompleted: {
        refreshList()
        if (startIndex >= 0 && startIndex < ruleItems.length) {
            selectedIndex = startIndex
            loadCurrent()
        }
    }

    Connections {
        target: AppController
        function onRulesChanged() {
            refreshList()
            if (selectedIndex >= ruleItems.length) selectedIndex = -1
            loadCurrent()
        }
    }


    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Rule list ─────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillHeight: true
            width: 260
            color: "#2b2d31"

            ColumnLayout {
                anchors { fill: parent; margins: 12 }
                spacing: 8

                Text { text: "Rules"; color: "#dbdee1"; font.pixelSize: 16; font.bold: true }

                Button {
                    text: "+ Add Rule"
                    Layout.fillWidth: true
                    onClicked: {
                        var idx = AppController.addRule({ "name": "New Rule" })
                        root.selectedIndex = idx
                        root.loadCurrent()
                    }
                    background: Rectangle { radius: 6; color: parent.hovered ? "#4752c4" : "#5865f2" }
                    contentItem: Text {
                        text: parent.text; color: "white"
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    implicitHeight: 34
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: root.ruleItems
                    clip: true
                    spacing: 2

                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        height: 40
                        radius: 6
                        color: root.selectedIndex === modelData.index
                               ? "#5865f233"
                               : (hoverArea.containsMouse ? "#3c3f4566" : "transparent")

                        RowLayout {
                            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8 }
                            spacing: 6
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: modelData.enabled ? "#23a55a" : "#ed4245"
                            }
                            Text {
                                text: modelData.name
                                color: "#dbdee1"; font.pixelSize: 13
                                Layout.fillWidth: true; elide: Text.ElideRight
                            }
                        }
                        MouseArea {
                            id: hoverArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { root.selectedIndex = modelData.index; root.loadCurrent() }
                        }
                    }
                }
            }
        }

        // ── Simplified editor ─────────────────────────────────────────────────
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            visible: root.selectedIndex >= 0

            ColumnLayout {
                width: parent.width
                anchors { top: parent.top; left: parent.left; right: parent.right; margins: 24 }
                spacing: 16

                Text { text: "Edit Rule"; color: "#dbdee1"; font.pixelSize: 18; font.bold: true }

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

                // 1 — Main line — bracket-free builder ("When here, show [verb] [value]")
                Label2 { text: "Main line (what Discord shows)" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    function build() {
                        var tok = fillCombo.currentValue || ""
                        var verb = verbCombo.editText.trim()
                        var tmpl = (verb + (tok ? (verb ? " " : "") + tok : "")).trim()
                        root.setField("activityNameTemplate", tmpl)
                        root.loadCurrent()
                    }
                    ComboBox {
                        id: verbCombo
                        Layout.preferredWidth: 150
                        editable: true
                        // Free-text: pick one or type your own verb (e.g. "Grinding").
                        model: ["Watching", "Playing", "Listening to", "Browsing", "Coding", "Streaming", ""]
                        onActivated: parent.build()
                        onAccepted: parent.build()
                    }
                    ComboBox {
                        id: fillCombo
                        Layout.fillWidth: true
                        textRole: "label"
                        valueRole: "token"
                        // Source-filtered: RuneScape rules see RuneScape tokens only,
                        // browser rules see browser tokens only, etc.
                        model: root.tokenModel(root.current)
                        onActivated: parent.build()
                    }
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: "Tip: the verb box is free-text — pick one or type your own (e.g. \"Grinding\"). " +
                          "The second box only lists values that fit this rule’s source (" + root.ruleSource(root.current) + ")."
                    color: "#949ba4"; font.pixelSize: 11
                }
                // Editable raw template (source of truth) + live preview
                TextField {
                    Layout.fillWidth: true
                    text: root.current.activityNameTemplate || ""
                    placeholderText: "e.g. Watching {{browser.label}}"
                    onTextEdited: root.setField("activityNameTemplate", text)
                    color: "#dbdee1"
                    background: Rectangle { radius: 4; color: "#1e1f22" }
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: {
                        var t = root.current.activityNameTemplate || ""
                        var shown = AppController.previewTemplate(t)
                        return "▶  Shows right now: " + (shown && shown.length ? shown : "(nothing yet — pick a value or switch to that app)")
                    }
                    color: "#949ba4"; font.pixelSize: 11
                }

                // 2 — Public / Private
                RowLayout {
                    spacing: 12
                    Switch {
                        checked: (root.current.privacyLevel || 0) === 2
                        onToggled: root.setField("privacyLevel", checked ? 2 : 0)
                    }
                    Text {
                        text: ((root.current.privacyLevel || 0) === 2)
                              ? "Private — this window will not show its details"
                              : "Public — this window shows its details"
                        color: "#dbdee1"; font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // 3 — Include extra detail
                Label2 { text: "Extra detail (side panel)" }
                RowLayout {
                    spacing: 12
                    Switch {
                        id: extraSwitch
                        checked: (root.current.stateTemplate || "") !== ""
                        onToggled: if (!checked) root.setField("stateTemplate", "")
                    }
                    ComboBox {
                        id: extraCombo
                        Layout.fillWidth: true
                        visible: extraSwitch.checked
                        enabled: count > 0
                        model: AppController.availableContextFields
                        textRole: "label"
                        displayText: count === 0 ? "Nothing extra available for this window"
                                                 : currentText
                        onActivated: root.setField("stateTemplate", model[currentIndex].token)
                    }
                }
                Text {
                    visible: extraSwitch.checked
                    text: "Currently: " + (root.current.stateTemplate || "(none)")
                    color: "#949ba4"; font.pixelSize: 11
                }

                // 4 — Icon (image URL)
                Label2 { text: "Icon (image URL)" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        width: 48; height: 48; radius: 6; color: "#1e1f22"; clip: true
                        Image {
                            id: ruleArt
                            anchors.fill: parent; anchors.margins: 1
                            fillMode: Image.PreserveAspectCrop
                            source: root.current.largeImageKey
                                    ? AppController.artSourceForKey(root.current.largeImageKey) : ""
                            visible: source !== "" && status === Image.Ready
                        }
                        Text { anchors.centerIn: parent; visible: !ruleArt.visible; text: "—"; color: "#4f5660" }
                    }
                    TextField {
                        Layout.fillWidth: true
                        text: root.current.largeImageKey || ""
                        placeholderText: "https://raw.githubusercontent.com/.../icon.png"
                        onEditingFinished: root.setField("largeImageKey", text)
                        color: "#dbdee1"
                        background: Rectangle { radius: 4; color: "#1e1f22" }
                    }
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: "Paste a public image URL (e.g. a raw GitHub link). Square PNGs look best — it shows above and on Discord."
                    color: "#949ba4"; font.pixelSize: 11
                }

                // ── Advanced (collapsed) ──────────────────────────────────────
                Button {
                    text: (root.advancedOpen ? "▾  " : "▸  ") + "Advanced"
                    flat: true
                    onClicked: root.advancedOpen = !root.advancedOpen
                    contentItem: Text { text: parent.text; color: "#949ba4"; font.pixelSize: 12 }
                    background: Item {}
                }

                ColumnLayout {
                    visible: root.advancedOpen
                    Layout.fillWidth: true
                    spacing: 10

                    AdvRow { label: "Priority";          value: String(root.current.priority || 100);    isNum: true; onCommit: root.setField("priority", parseInt(v)) }
                    AdvRow { label: "Process name";      value: root.current.matchProcessName || "";     onCommit: root.setField("matchProcessName", v) }
                    AdvRow { label: "Executable path";   value: root.current.matchExecutablePath || "";  onCommit: root.setField("matchExecutablePath", v) }
                    AdvRow { label: "Window title";      value: root.current.matchWindowTitle || "";     onCommit: root.setField("matchWindowTitle", v) }
                    AdvRow { label: "Browser domain";    value: root.current.matchBrowserDomain || "";   onCommit: root.setField("matchBrowserDomain", v) }
                    AdvRow { label: "Integration source";value: root.current.matchIntegrationSource || "";onCommit: root.setField("matchIntegrationSource", v) }
                    AdvRow { label: "Details template";  value: root.current.detailsTemplate || "";      onCommit: root.setField("detailsTemplate", v) }
                    AdvRow { label: "State template";    value: root.current.stateTemplate || "";        onCommit: root.setField("stateTemplate", v) }
                    AdvRow { label: "Large image key";   value: root.current.largeImageKey || "";        onCommit: root.setField("largeImageKey", v) }
                    AdvRow { label: "Small image key";   value: root.current.smallImageKey || "";        onCommit: root.setField("smallImageKey", v) }
                }

                // ── Actions ───────────────────────────────────────────────────
                RowLayout {
                    spacing: 12
                    Button {
                        text: "Save"
                        onClicked: { AppController.saveRules(); uploadHint.visible = false }
                        background: Rectangle { radius: 6; color: parent.hovered ? "#4752c4" : "#5865f2" }
                        contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        implicitWidth: 100; implicitHeight: 36
                    }
                    Button {
                        text: "Delete"
                        onClicked: { AppController.deleteRule(root.selectedIndex); root.selectedIndex = -1 }
                        background: Rectangle { radius: 6; color: parent.hovered ? "#b32222" : "#ed4245" }
                        contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        implicitWidth: 100; implicitHeight: 36
                    }
                }

                Item { height: 24 }
            }
        }

        // Empty state
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.selectedIndex < 0
            Text {
                anchors.centerIn: parent
                text: "Select a rule to edit, or click + Add Rule."
                color: "#4f5660"; font.pixelSize: 14
            }
        }
    }

    // ── Inline components ─────────────────────────────────────────────────────
    component Label2: Text {
        color: "#949ba4"
        font.pixelSize: 11
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
        Layout.fillWidth: true
    }

    component AdvRow: RowLayout {
        property string label: ""
        property string value: ""
        property bool isNum: false
        signal commit(string v)
        spacing: 12
        Text { text: label; color: "#dbdee1"; font.pixelSize: 12; Layout.minimumWidth: 150 }
        TextField {
            Layout.fillWidth: true
            text: value
            color: "#dbdee1"
            background: Rectangle { radius: 4; color: "#1e1f22" }
            onEditingFinished: commit(text)
        }
    }
}

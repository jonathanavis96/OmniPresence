// CustomPage.qml — Manual presence override ("Custom" tab).
//
// Lets the user define one or more hand-authored presets and either show one
// of them permanently ("single" mode) or rotate through several on a timer
// ("cycle" mode). When enabled this overrides all rules, idle detection, and
// pause. Presets are read/written through AppController's custom-override
// bridge — mirrors RulesPage.qml's structure and styling.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Page {
    id: root
    readonly property string pageId: "CustomPage.qml"
    background: Rectangle { color: "#313338" }

    property int selectedIndex: -1
    property var presetItems: []
    property var current: ({})
    property bool advancedOpen: false
    property string uploadStatus: ""

    function refreshList() {
        presetItems = AppController.customPresetsList()
    }
    function loadCurrent() {
        current = (selectedIndex >= 0) ? AppController.customPresetAt(selectedIndex) : ({})
    }
    function setField(field, value) {
        if (selectedIndex >= 0) AppController.updateCustomPresetField(selectedIndex, field, value)
    }

    Component.onCompleted: refreshList()

    Connections {
        target: AppController
        function onCustomChanged() {
            refreshList()
            if (selectedIndex >= presetItems.length) selectedIndex = -1
            loadCurrent()
        }
        function onCustomUploadFinished(ok, message) {
            root.uploadStatus = ok ? "✓ Uploaded" : ("⚠ " + message)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Top strip: master enable + mode selector ────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: topCol.implicitHeight + 24
            color: "#2b2d31"

            ColumnLayout {
                id: topCol
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 16; rightMargin: 16 }
                spacing: 8

                RowLayout {
                    spacing: 12
                    Switch {
                        id: enableSwitch
                        checked: AppController.customEnabled
                        onToggled: AppController.customEnabled = checked
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text { text: "Enable Custom Override"; color: "#dbdee1"; font.pixelSize: 14; font.bold: true }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "When on, this replaces all rules, idle detection, and pause — Discord always shows the preset(s) below."
                            color: "#949ba4"; font.pixelSize: 11
                        }
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Mode:"; color: "#dbdee1"; font.pixelSize: 12 }
                    RadioButton {
                        text: "Single preset"
                        checked: AppController.customMode === "single"
                        onToggled: if (checked) AppController.customMode = "single"
                    }
                    RadioButton {
                        text: "Cycle presets"
                        checked: AppController.customMode === "cycle"
                        onToggled: if (checked) AppController.customMode = "cycle"
                    }
                    SpinBox {
                        id: intervalSpin
                        visible: AppController.customMode === "cycle"
                        from: 1
                        to: 3600
                        value: AppController.customIntervalSeconds
                        textFromValue: function(value, locale) { return value + " s" }
                        valueFromText: function(text, locale) { return parseInt(text) || 1 }
                        onValueModified: AppController.customIntervalSeconds = value
                    }
                }
            }
        }

        // ── Two-pane body ─────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── Preset list ──────────────────────────────────────────────────
            Rectangle {
                Layout.fillHeight: true
                width: 260
                color: "#2b2d31"

                ColumnLayout {
                    anchors { fill: parent; margins: 12 }
                    spacing: 8

                    Text { text: "Presets"; color: "#dbdee1"; font.pixelSize: 16; font.bold: true }

                    Button {
                        text: "+ Add Preset"
                        Layout.fillWidth: true
                        onClicked: {
                            root.selectedIndex = AppController.addCustomPreset({ "label": "Custom" })
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
                        model: root.presetItems
                        clip: true
                        spacing: 2

                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width
                            height: 48
                            radius: 6
                            color: root.selectedIndex === modelData.index
                                   ? "#5865f233"
                                   : (hoverArea.containsMouse ? "#3c3f4566" : "transparent")

                            MouseArea {
                                id: hoverArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: { root.selectedIndex = modelData.index; root.loadCurrent() }
                            }

                            RowLayout {
                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8 }
                                spacing: 6

                                Rectangle {
                                    width: 8; height: 8; radius: 4
                                    visible: AppController.customMode === "cycle"
                                    color: modelData.includeInCycle ? "#23a55a" : "#4f5660"
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Text {
                                        text: modelData.label
                                        color: "#dbdee1"; font.pixelSize: 13
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                    }
                                    Text {
                                        text: modelData.name && modelData.name.length ? modelData.name : "(no name set)"
                                        color: "#949ba4"; font.pixelSize: 10
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                    }
                                }

                                Button {
                                    text: "▲"
                                    flat: true
                                    enabled: modelData.index > 0
                                    implicitWidth: 22; implicitHeight: 22
                                    onClicked: AppController.reorderCustomPreset(modelData.index, modelData.index - 1)
                                    contentItem: Text {
                                        text: parent.text
                                        color: parent.enabled ? "#949ba4" : "#4f5660"
                                        font.pixelSize: 10
                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Item {}
                                }
                                Button {
                                    text: "▼"
                                    flat: true
                                    enabled: modelData.index < root.presetItems.length - 1
                                    implicitWidth: 22; implicitHeight: 22
                                    onClicked: AppController.reorderCustomPreset(modelData.index, modelData.index + 1)
                                    contentItem: Text {
                                        text: parent.text
                                        color: parent.enabled ? "#949ba4" : "#4f5660"
                                        font.pixelSize: 10
                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Item {}
                                }
                            }
                        }
                    }
                }
            }

            // ── Editor ───────────────────────────────────────────────────────
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                visible: root.selectedIndex >= 0

                ColumnLayout {
                    width: parent.width
                    anchors { top: parent.top; left: parent.left; right: parent.right; margins: 24 }
                    spacing: 16

                    Text { text: "Edit Preset"; color: "#dbdee1"; font.pixelSize: 18; font.bold: true }

                    // 0 — Label (list display name)
                    Label2 { text: "Label (shown in the list here — not on Discord)" }
                    TextField {
                        Layout.fillWidth: true
                        text: root.current.label || ""
                        placeholderText: "e.g. Working, AFK, Streaming"
                        onTextEdited: root.setField("label", text)
                        color: "#dbdee1"
                        background: Rectangle { radius: 4; color: "#1e1f22" }
                    }

                    // 1 — Name (Discord's activity name — required)
                    Label2 { text: "Name (what Discord shows)" }
                    TextField {
                        Layout.fillWidth: true
                        text: root.current.name || ""
                        placeholderText: "e.g. hello"
                        onTextEdited: root.setField("name", text)
                        color: "#dbdee1"
                        background: Rectangle { radius: 4; color: "#1e1f22" }
                    }
                    Text {
                        visible: (root.current.name || "") === ""
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: "Required — a blank name means this preset won't publish."
                        color: "#ed4245"; font.pixelSize: 11
                    }

                    // 2 — Details / State
                    Label2 { text: "Details" }
                    TextField {
                        Layout.fillWidth: true
                        text: root.current.details || ""
                        onTextEdited: root.setField("details", text)
                        color: "#dbdee1"
                        background: Rectangle { radius: 4; color: "#1e1f22" }
                    }
                    Label2 { text: "State" }
                    TextField {
                        Layout.fillWidth: true
                        text: root.current.state || ""
                        onTextEdited: root.setField("state", text)
                        color: "#dbdee1"
                        background: Rectangle { radius: 4; color: "#1e1f22" }
                    }

                    // 3 — Activity type
                    Label2 { text: "Activity type" }
                    ComboBox {
                        id: activityCombo
                        Layout.fillWidth: true
                        model: ["Playing", "Listening", "Watching", "Competing"]
                        currentIndex: Math.max(0, model.indexOf(root.current.activityType || "Playing"))
                        onActivated: root.setField("activityType", model[currentIndex])
                    }

                    // 4 — Single-mode: which preset is active
                    RadioButton {
                        visible: AppController.customMode === "single"
                        text: "Show this preset (active)"
                        checked: AppController.customActiveIndex === root.selectedIndex
                        onToggled: if (checked) AppController.customActiveIndex = root.selectedIndex
                    }

                    // 5 — Cycle-mode: include in rotation
                    RowLayout {
                        visible: AppController.customMode === "cycle"
                        spacing: 12
                        Switch {
                            checked: root.current.includeInCycle === true
                            onToggled: root.setField("includeInCycle", checked)
                        }
                        Text {
                            text: "Include in cycle"
                            color: "#dbdee1"; font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    // 6 — Icon
                    Label2 { text: "Icon" }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Rectangle {
                            width: 48; height: 48; radius: 6; color: "#1e1f22"; clip: true
                            Image {
                                id: customArt
                                anchors.fill: parent; anchors.margins: 1
                                fillMode: Image.PreserveAspectCrop
                                source: root.current.largeImageKey
                                        ? AppController.artSourceForKey(root.current.largeImageKey) : ""
                                visible: source !== "" && status === Image.Ready
                            }
                            Text { anchors.centerIn: parent; visible: !customArt.visible; text: "—"; color: "#4f5660" }
                            DropArea {
                                anchors.fill: parent
                                onDropped: function(drop) {
                                    if (drop.hasUrls && drop.urls.length > 0 && root.selectedIndex >= 0) {
                                        var path = drop.urls[0].toString().replace(/^file:\/\//, "")
                                        root.uploadStatus = "Uploading…"
                                        AppController.uploadPresetImage(root.selectedIndex, path)
                                    }
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            TextField {
                                Layout.fillWidth: true
                                text: root.current.largeImageKey || ""
                                placeholderText: "https://raw.githubusercontent.com/.../icon.png"
                                onEditingFinished: root.setField("largeImageKey", text)
                                color: "#dbdee1"
                                background: Rectangle { radius: 4; color: "#1e1f22" }
                            }
                            ComboBox {
                                id: libraryCombo
                                Layout.fillWidth: true
                                textRole: "label"
                                model: AppController.customImageLibrary()
                                displayText: "Pick from library…"
                                onActivated: {
                                    var item = model[currentIndex]
                                    if (item) root.setField("largeImageKey", item.url)
                                }
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: "Drag an image here to upload, pick from your library, or paste a URL."
                        color: "#949ba4"; font.pixelSize: 11
                    }
                    Text {
                        visible: root.uploadStatus !== ""
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: root.uploadStatus
                        color: root.uploadStatus.indexOf("⚠") === 0 ? "#ed4245" : "#23a55a"
                        font.pixelSize: 11
                    }

                    // 7 — Live preview
                    Text {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: {
                            var n = root.current.name || "(no name)"
                            var d = root.current.details || ""
                            var s = root.current.state || ""
                            var line = "▶  Discord will show: " + n
                            if (d) line += " — " + d
                            if (s) line += " — " + s
                            return line
                        }
                        color: "#949ba4"; font.pixelSize: 11
                    }

                    // ── Advanced (collapsed) ──────────────────────────────────
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

                        AdvRow { label: "Large image text"; value: root.current.largeImageText || ""; onCommit: root.setField("largeImageText", v) }
                        AdvRow { label: "Small image key";  value: root.current.smallImageKey || "";  onCommit: root.setField("smallImageKey", v) }
                        AdvRow { label: "Small image text"; value: root.current.smallImageText || ""; onCommit: root.setField("smallImageText", v) }
                    }

                    // ── Actions ───────────────────────────────────────────────
                    RowLayout {
                        spacing: 12
                        Button {
                            text: "Done"
                            onClicked: {} // every field edit already persists immediately
                            background: Rectangle { radius: 6; color: parent.hovered ? "#4752c4" : "#5865f2" }
                            contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            implicitWidth: 100; implicitHeight: 36
                        }
                        Button {
                            text: "Delete"
                            onClicked: { AppController.deleteCustomPreset(root.selectedIndex); root.selectedIndex = -1 }
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
                    text: "Select a preset to edit, or click + Add Preset."
                    color: "#4f5660"; font.pixelSize: 14
                }
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

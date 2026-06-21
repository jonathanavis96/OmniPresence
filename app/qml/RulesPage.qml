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

    FileDialog {
        id: photoDialog
        title: "Choose an image for this rule"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.bmp)"]
        onAccepted: {
            var key = AppController.importPhoto(root.selectedIndex, selectedFile)
            if (key !== "") {
                root.loadCurrent()
                uploadHint.text = "Saved as \"" + key + "\". Drop the file Explorer just "
                    + "revealed into the Art Assets page that opened, then Save."
                uploadHint.visible = true
            }
        }
    }

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

                // 1 — Main line
                Label2 { text: "Main line (what Discord shows)" }
                TextField {
                    Layout.fillWidth: true
                    text: root.current.activityNameTemplate || ""
                    placeholderText: "e.g. RuneLight – {{runelite.activity}}"
                    onTextEdited: root.setField("activityNameTemplate", text)
                    color: "#dbdee1"
                    background: Rectangle { radius: 4; color: "#1e1f22" }
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

                // 4 — Image
                Label2 { text: "Image" }
                RowLayout {
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
                    ComboBox {
                        Layout.fillWidth: true
                        model: AppController.artKeys()
                        currentIndex: model.indexOf(root.current.largeImageKey || "")
                        onActivated: root.setField("largeImageKey", model[currentIndex])
                    }
                    Button {
                        text: "Add photo…"
                        onClicked: photoDialog.open()
                        background: Rectangle { radius: 6; color: parent.hovered ? "#4752c4" : "#5865f2" }
                        contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        implicitHeight: 34; implicitWidth: 110
                    }
                    Button {
                        text: "Generate"
                        onClicked: genPopup.open()
                        background: Rectangle { radius: 6; color: parent.hovered ? "#3a3d44" : "#2b2d31"; border.color: "#5865f2"; border.width: 1 }
                        contentItem: Text { text: parent.text; color: "#dbdee1"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        implicitHeight: 34; implicitWidth: 96
                    }
                }
                Text {
                    id: uploadHint
                    visible: false
                    Layout.fillWidth: true
                    color: "#faa81a"; font.pixelSize: 12; wrapMode: Text.WordWrap
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

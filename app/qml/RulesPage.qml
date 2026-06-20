// RulesPage.qml — Browse, add, edit and delete presence rules.
// Rule data is read/written via AppController → ConfigStore.
// Full CRUD is stubbed here; the backend C++ signals update the rule list.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    readonly property string pageId: "RulesPage.qml"
    background: Rectangle { color: "#313338" }

    // Placeholder model until the C++ RuleSet model is exposed via QAbstractListModel.
    // TODO: Replace with a proper QAbstractListModel exposed from ConfigStore.
    ListModel {
        id: ruleModel
        // Populated at runtime from AppController.configStore().ruleSet()
        // For now the list starts empty and is populated via the editor below.
    }

    // ── Selected rule index for the editor panel ──────────────────────────────
    property int selectedIndex: -1

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

                Text {
                    text: "Rules"
                    color: "#dbdee1"
                    font.pixelSize: 16
                    font.bold: true
                }

                Button {
                    text: "+ Add Rule"
                    Layout.fillWidth: true
                    onClicked: {
                        ruleModel.append({
                            name: "New Rule",
                            enabled: true,
                            priority: 100,
                            matchProcessName: "",
                            matchWindowTitle: "",
                            detailsTemplate: "",
                            stateTemplate: "",
                        })
                        root.selectedIndex = ruleModel.count - 1
                    }
                    background: Rectangle { radius: 6; color: parent.hovered ? "#4752c4" : "#5865f2" }
                    contentItem: Text {
                        text: parent.text; color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment:   Text.AlignVCenter
                    }
                    implicitHeight: 34
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: ruleModel
                    clip: true
                    spacing: 2

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 40
                        radius: 6
                        color: root.selectedIndex === index
                               ? "#5865f233"
                               : (hoverArea.containsMouse ? "#3c3f4566" : "transparent")

                        RowLayout {
                            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8 }
                            spacing: 6

                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: model.enabled ? "#23a55a" : "#ed4245"
                            }
                            Text {
                                text: model.name || "(unnamed)"
                                color: "#dbdee1"
                                font.pixelSize: 13
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: String(model.priority)
                                color: "#949ba4"
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            id: hoverArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedIndex = index
                        }
                    }
                }
            }
        }

        // ── Rule editor ───────────────────────────────────────────────────────
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            visible: root.selectedIndex >= 0 && root.selectedIndex < ruleModel.count

            ColumnLayout {
                anchors { top: parent.top; left: parent.left; right: parent.right; margins: 24 }
                spacing: 14

                Text { text: "Edit Rule"; color: "#dbdee1"; font.pixelSize: 18; font.bold: true }

                // ── Basic ─────────────────────────────────────────────────────
                SectionHeader { text: "Basic" }

                FieldRow {
                    label: "Name"
                    field: TextField {
                        text: root.selectedIndex >= 0 ? ruleModel.get(root.selectedIndex).name : ""
                        onTextChanged: if (root.selectedIndex >= 0) ruleModel.setProperty(root.selectedIndex, "name", text)
                    }
                }

                FieldRow {
                    label: "Priority"
                    field: SpinBox {
                        from: 0; to: 9999; stepSize: 10
                        value: root.selectedIndex >= 0 ? ruleModel.get(root.selectedIndex).priority : 100
                        onValueChanged: if (root.selectedIndex >= 0) ruleModel.setProperty(root.selectedIndex, "priority", value)
                    }
                }

                FieldRow {
                    label: "Enabled"
                    field: Switch {
                        checked: root.selectedIndex >= 0 ? ruleModel.get(root.selectedIndex).enabled : true
                        onCheckedChanged: if (root.selectedIndex >= 0) ruleModel.setProperty(root.selectedIndex, "enabled", checked)
                    }
                }

                // ── Match criteria ────────────────────────────────────────────
                SectionHeader { text: "Match Criteria" }

                FieldRow { label: "Process name";      field: TextField { placeholderText: "e.g. chrome.exe" } }
                FieldRow { label: "Executable path";   field: TextField { placeholderText: "substring match" } }
                FieldRow { label: "Window title";      field: TextField { placeholderText: "substring or regex" } }
                FieldRow { label: "Use regex";         field: Switch { } }
                FieldRow { label: "Browser domain";    field: TextField { placeholderText: "e.g. youtube.com" } }
                FieldRow { label: "Browser category";  field: TextField { placeholderText: "e.g. Social" } }
                FieldRow { label: "Integration source";field: TextField { placeholderText: "runelite / vscode / terminal" } }

                // ── Output templates ──────────────────────────────────────────
                SectionHeader { text: "Output Templates" }

                Text {
                    text: "Available: {{app.name}} {{window.title}} {{process.name}} {{browser.domain}}\n"
                        + "{{browser.category}} {{terminal.cwd}} {{terminal.repo}} {{terminal.command_summary}}\n"
                        + "{{vscode.workspace}} {{runelite.activity}} {{runelite.target}} {{runelite.skill}}\n"
                        + "{{runelite.location}} {{runelite.confidence}}   |   Fallback: {{a or b}}"
                    color: "#949ba4"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                FieldRow { label: "Activity Name";     field: TextField { placeholderText: "e.g. {{app.name}}" } }
                FieldRow { label: "Details";           field: TextField { placeholderText: "e.g. Editing {{window.title}}" } }
                FieldRow { label: "State";             field: TextField { placeholderText: "e.g. {{terminal.repo or vscode.workspace}}" } }
                FieldRow { label: "Large image key";   field: TextField { } }
                FieldRow { label: "Large image text";  field: TextField { } }
                FieldRow { label: "Small image key";   field: TextField { } }
                FieldRow { label: "Small image text";  field: TextField { } }

                // ── Timestamp + privacy ───────────────────────────────────────
                SectionHeader { text: "Behaviour" }

                FieldRow {
                    label: "Timestamp mode"
                    field: ComboBox {
                        model: ["None", "StartNow", "Keep", "CategoryChange"]
                        currentIndex: 3
                    }
                }

                FieldRow {
                    label: "Privacy level"
                    field: ComboBox {
                        model: ["Public", "DomainOnly", "Private"]
                    }
                }

                // ── Actions ───────────────────────────────────────────────────
                RowLayout {
                    spacing: 12

                    Button {
                        text: "Save"
                        onClicked: AppController.saveConfig()
                        background: Rectangle { radius: 6; color: parent.hovered ? "#4752c4" : "#5865f2" }
                        contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        implicitWidth: 100; implicitHeight: 36
                    }
                    Button {
                        text: "Delete"
                        onClicked: {
                            if (root.selectedIndex >= 0) ruleModel.remove(root.selectedIndex)
                            root.selectedIndex = -1
                        }
                        background: Rectangle { radius: 6; color: parent.hovered ? "#b32222" : "#ed4245" }
                        contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        implicitWidth: 100; implicitHeight: 36
                    }
                }

                Item { height: 24 }
            }
        }

        // Empty-state placeholder when no rule selected
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.selectedIndex < 0 || root.selectedIndex >= ruleModel.count

            Text {
                anchors.centerIn: parent
                text: "Select a rule to edit, or click + Add Rule."
                color: "#4f5660"
                font.pixelSize: 14
            }
        }
    }

    // ── Inline components ─────────────────────────────────────────────────────
    component SectionHeader: Text {
        color: "#949ba4"
        font.pixelSize: 11
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
        topPadding: 8
    }

    component FieldRow: RowLayout {
        property string label: ""
        default property alias field: fieldSlot.children

        spacing: 12

        Text {
            text: label
            color: "#dbdee1"
            font.pixelSize: 13
            Layout.minimumWidth: 140
            verticalAlignment: Text.AlignVCenter
        }

        Item {
            id: fieldSlot
            Layout.fillWidth: true
            implicitHeight: children.length > 0 ? children[0].implicitHeight : 0
        }
    }
}

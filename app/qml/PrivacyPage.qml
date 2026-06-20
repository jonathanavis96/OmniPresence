// PrivacyPage.qml — Global privacy controls + per-source whitelist toggles.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    readonly property string pageId: "PrivacyPage.qml"
    background: Rectangle { color: "#313338" }

    ScrollView {
        id: sv
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            // width bound to availableWidth; anchoring to the ScrollView's
            // Flickable contentItem (width 0) would collapse the layout.
            x: 24; y: 24
            width: sv.availableWidth - 48
            spacing: 20

            Text {
                text: "Privacy"
                color: "#dbdee1"
                font.pixelSize: 20
                font.bold: true
                topPadding: 8
            }

            // ── Global controls ───────────────────────────────────────────────
            SectionCard {
                Layout.fillWidth: true
                cardTitle: "Global Controls"

                ColumnLayout {
                    spacing: 12

                    ToggleRow {
                        label:       "Pause all presence updates"
                        description: "Clears your Discord presence and stops all updates until resumed."
                        checked:     AppController.paused
                        onToggled:   (v) => v ? AppController.pause() : AppController.resume()
                        accentColor: "#ed4245"
                    }

                    Rectangle { height: 1; Layout.fillWidth: true; color: "#3f4147" }

                    ToggleRow {
                        label:       "Private mode"
                        description: "Shows the private fallback (Computer / Working privately) instead of real activity."
                        checked:     AppController.privacyMode
                        onToggled:   (v) => AppController.setPrivacyMode(v)
                        accentColor: "#faa81a"
                    }
                }
            }

            // ── Browser whitelist ─────────────────────────────────────────────
            SectionCard {
                Layout.fillWidth: true
                cardTitle: "Browser Privacy Overrides"

                Text {
                    text: "When these patterns match the browser window title, the private fallback is used regardless of your rules. "
                        + "These are applied by adding a high-priority Private rule in the rule engine."
                    color: "#949ba4"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                // Each item: label, description, enabled by default
                // TODO: Wire these to ConfigStore privacy settings once the backing model is exposed.
                ColumnLayout {
                    spacing: 10

                    ToggleRow {
                        label: "Exact localhost titles (127.0.0.1, localhost)"
                        description: "Hides local dev environment tabs from your presence."
                        checked: true
                        accentColor: "#5865f2"
                    }
                    ToggleRow {
                        label: "Exact YouTube video titles"
                        description: "Shows only 'YouTube' instead of the full video title."
                        checked: false
                        accentColor: "#5865f2"
                    }
                    ToggleRow {
                        label: "Exact GitHub repo names"
                        description: "Hides repository names from your presence."
                        checked: false
                        accentColor: "#5865f2"
                    }
                    ToggleRow {
                        label: "ChatGPT / Claude / AI chat titles"
                        description: "Replaces AI assistant conversation titles with a generic label. Default: off."
                        checked: false
                        accentColor: "#5865f2"
                    }
                }
            }

            // ── Notes ─────────────────────────────────────────────────────────
            Text {
                text: "All privacy overrides are evaluated at priority 1 — they always win, regardless of rules."
                color: "#4f5660"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Item { height: 24 }
        }
    }

    // ── Inline components ─────────────────────────────────────────────────────
    component SectionCard: Rectangle {
        property string cardTitle: ""
        default property alias cardContent: cardCol.children

        radius: 8
        color: "#2b2d31"
        implicitHeight: cardCol.implicitHeight + 32
        Layout.fillWidth: true

        ColumnLayout {
            id: cardCol
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
            spacing: 12

            Text {
                text: cardTitle
                color: "#949ba4"
                font.pixelSize: 11
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
            }
        }
    }

    component ToggleRow: RowLayout {
        property string label: ""
        property string description: ""
        property bool   checked: false
        property color  accentColor: "#5865f2"
        signal toggled(bool value)

        spacing: 12
        Layout.fillWidth: true

        Switch {
            id: sw
            checked: parent.checked
            onCheckedChanged: parent.toggled(checked)

            indicator: Rectangle {
                implicitWidth: 40; implicitHeight: 22; radius: 11
                color: sw.checked ? parent.accentColor : "#4f5660"

                Rectangle {
                    width: 18; height: 18; radius: 9
                    color: "white"
                    anchors.verticalCenter: parent.verticalCenter
                    x: sw.checked ? parent.width - width - 2 : 2
                    Behavior on x { NumberAnimation { duration: 120 } }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: label
                color: "#dbdee1"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Text {
                text: description
                color: "#949ba4"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                visible: description !== ""
            }
        }
    }
}

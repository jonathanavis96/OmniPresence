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

            // ── Browser privacy (owned by the extension) ──────────────────────
            SectionCard {
                Layout.fillWidth: true
                cardTitle: "Browser Privacy"

                Text {
                    text: "Browser page titles are private by default — non-whitelisted sites "
                        + "only ever report their domain. To let a specific site show its title "
                        + "(e.g. a YouTube video name or the 'Show name from URL' label), open the "
                        + "OmniPresence Browser Bridge extension in Chrome and add that domain to "
                        + "its whitelist. That whitelist is the single place browser title privacy "
                        + "is controlled."
                    color: "#949ba4"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
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

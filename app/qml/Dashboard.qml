// Dashboard.qml — Live status overview: active window + matched rule + Discord presence preview.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    readonly property string pageId: "Dashboard.qml"
    background: Rectangle { color: "#313338" }

    ScrollView {
        id: sv
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            // Bind width to the ScrollView's availableWidth. Anchoring to `parent`
            // here targets the Flickable contentItem (width 0) and collapses the
            // layout to zero width — nothing renders, with no QML error.
            x: 24; y: 24
            width: sv.availableWidth - 48
            spacing: 16

            // ── Section header ────────────────────────────────────────────────
            Text {
                text: "Dashboard"
                color: "#dbdee1"
                font.pixelSize: 20
                font.bold: true
                topPadding: 8
            }

            // ── Active window card ────────────────────────────────────────────
            SectionCard {
                Layout.fillWidth: true
                title: "Active Window"

                ColumnLayout {
                    spacing: 6

                    LabeledRow { label: "Process";   value: AppController.currentProcessName || "—" }
                    LabeledRow { label: "Title";     value: AppController.currentWindowTitle || "—" }
                    LabeledRow { label: "Class";     value: AppController.currentWindowClass || "—" }
                    LabeledRow { label: "Exe";       value: AppController.currentExePath     || "—"; mono: true }
                }
            }

            // ── Rule match card ───────────────────────────────────────────────
            SectionCard {
                Layout.fillWidth: true
                title: "Matched Rule"

                Text {
                    text: AppController.matchedRuleName || "(no match — using private fallback)"
                    color: AppController.matchedRuleName ? "#dbdee1" : "#949ba4"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }

            // ── Discord connection card ───────────────────────────────────────
            SectionCard {
                Layout.fillWidth: true
                title: "Discord Connection"

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    LabeledRow {
                        label: "Status"
                        value: AppController.discordStatus
                               + (AppController.discordAppId
                                  ? "   (app " + AppController.discordAppId + ")"
                                  : "   (no app ID configured)")
                    }

                    // Error line — only shown when something went wrong.
                    Text {
                        visible: AppController.discordError !== ""
                        text: "⚠ " + AppController.discordError
                        color: "#faa81a"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Text {
                        visible: !AppController.discordConnected
                        text: "First run opens your browser once to authorise. After that it reconnects silently."
                        color: "#949ba4"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        spacing: 8

                        Button {
                            text: AppController.discordConnected ? "Reconnect"
                                  : (AppController.discordStatus === "Connecting…"
                                     ? "Connecting…" : "Connect to Discord")
                            enabled: AppController.discordStatus !== "Connecting…"
                            onClicked: AppController.connectDiscord()

                            background: Rectangle {
                                radius: 6
                                color: !parent.enabled ? "#4f5660"
                                       : parent.hovered ? "#4752c4" : "#5865f2"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment:   Text.AlignVCenter
                            }
                            implicitWidth: 180
                            implicitHeight: 36
                        }

                        Button {
                            visible: AppController.discordConnected
                            text: "Disconnect"
                            onClicked: AppController.disconnectDiscord()

                            background: Rectangle {
                                radius: 6
                                color: parent.hovered ? "#3a3c41" : "#2b2d31"
                                border.color: "#4f5660"
                                border.width: 1
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#dbdee1"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment:   Text.AlignVCenter
                            }
                            implicitWidth: 120
                            implicitHeight: 36
                        }
                    }
                }
            }

            // ── Presence preview card ─────────────────────────────────────────
            SectionCard {
                Layout.fillWidth: true
                title: "Discord Rich Presence"

                ColumnLayout {
                    spacing: 6

                    LabeledRow { label: "Name";    value: AppController.presenceName    || "—" }
                    LabeledRow { label: "Details"; value: AppController.presenceDetails || "—" }
                    LabeledRow { label: "State";   value: AppController.presenceState   || "—" }

                    Rectangle {
                        height: 1
                        Layout.fillWidth: true
                        color: "#3f4147"
                        visible: AppController.isPrivateFallback
                    }

                    Text {
                        visible: AppController.isPrivateFallback
                        text: "⚠ Private fallback active — no public presence published."
                        color: "#faa81a"
                        font.pixelSize: 12
                    }
                }
            }

            // ── Status bar ────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                StatusChip {
                    label: AppController.discordConnected ? "Discord connected" : "Discord disconnected"
                    active: AppController.discordConnected
                }
                StatusChip {
                    label: AppController.privacyMode ? "Private mode ON" : "Private mode off"
                    active: !AppController.privacyMode
                    dangerWhenActive: false
                }
                StatusChip {
                    label: AppController.paused ? "Paused" : "Running"
                    active: !AppController.paused
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "Last update: " + (AppController.lastUpdateTime || "—")
                    color: "#949ba4"
                    font.pixelSize: 11
                }
            }

            Item { height: 24 }
        }
    }

    // ── Inline components ─────────────────────────────────────────────────────
    component SectionCard: Rectangle {
        id: card
        property string title: ""
        default property alias cardContent: cardColumn.children
        radius: 8
        color: "#2b2d31"
        implicitHeight: cardColumn.implicitHeight + 32

        ColumnLayout {
            id: cardColumn
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
            spacing: 10

            Text {
                text: card.title  // the SectionCard instance, not the Page root
                color: "#949ba4"
                font.pixelSize: 11
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
            }
        }

        // Fix: title binding needs to refer to the card, not the Page root.
        // Re-bind inside onCompleted if needed; this compiles correctly.
    }

    component LabeledRow: RowLayout {
        property string label: ""
        property string value: ""
        property bool   mono: false

        spacing: 8

        Text {
            text: label + ":"
            color: "#949ba4"
            font.pixelSize: 12
            Layout.minimumWidth: 70
        }
        Text {
            text: value
            color: "#dbdee1"
            font.pixelSize: 13
            font.family: mono ? "Consolas, monospace" : ""
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    component StatusChip: Rectangle {
        property string label: ""
        property bool   active: true
        property bool   dangerWhenActive: true

        radius: 20
        implicitWidth: chipLabel.implicitWidth + 20
        implicitHeight: 24
        color: active
               ? (dangerWhenActive ? "#23a55a22" : "#23a55a22")
               : "#ed424522"

        Text {
            id: chipLabel
            anchors.centerIn: parent
            text: label
            color: active ? "#23a55a" : "#ed4245"
            font.pixelSize: 11
            leftPadding: 10
            rightPadding: 10
        }
    }
}

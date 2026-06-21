// PreviewPage.qml — Live presence preview and manual "Publish / Test" trigger.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    readonly property string pageId: "PreviewPage.qml"
    background: Rectangle { color: "#313338" }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 20

        Text {
            text: "Presence Preview"
            color: "#dbdee1"
            font.pixelSize: 20
            font.bold: true
        }

        Text {
            text: "This is what your Discord profile will show based on the current window and matched rule."
            color: "#949ba4"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ── Discord-style presence card ───────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            radius: 8
            color: "#2b2d31"
            implicitHeight: cardContent.implicitHeight + 40

            ColumnLayout {
                id: cardContent
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
                spacing: 4

                Text {
                    text: AppController.isPrivateFallback ? "PRIVATE" : "PLAYING"
                    color: "#949ba4"
                    font.pixelSize: 10
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 1.2
                }

                // Real large image (from the published payload) + text block.
                RowLayout {
                    spacing: 12
                    Layout.fillWidth: true

                    Rectangle {
                        width: 60; height: 60; radius: 8
                        color: "#1e1f22"
                        clip: true

                        Image {
                            id: bigArt
                            anchors.fill: parent
                            anchors.margins: 1
                            fillMode: Image.PreserveAspectCrop
                            source: AppController.presenceLargeImageSource
                            visible: source !== "" && status === Image.Ready
                        }
                        // Neutral placeholder only when there is genuinely no art.
                        Text {
                            anchors.centerIn: parent
                            visible: !bigArt.visible
                            text: "—"
                            color: "#4f5660"
                            font.pixelSize: 22
                        }

                        // Real small image overlay (from the payload).
                        Rectangle {
                            width: 22; height: 22; radius: 11
                            color: "#2b2d31"
                            anchors { bottom: parent.bottom; right: parent.right; margins: -4 }
                            clip: true
                            visible: smallArt.visible

                            Image {
                                id: smallArt
                                anchors.fill: parent
                                anchors.margins: 2
                                fillMode: Image.PreserveAspectCrop
                                source: AppController.presenceSmallImageSource
                                visible: source !== "" && status === Image.Ready
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: AppController.presenceName || "(no name)"
                            color: "#dbdee1"
                            font.pixelSize: 15
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: AppController.presenceDetails || ""
                            color: "#dbdee1"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            visible: AppController.presenceDetails !== ""
                            Layout.fillWidth: true
                        }
                        Text {
                            text: AppController.presenceState || ""
                            color: "#949ba4"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            visible: AppController.presenceState !== ""
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // ── Meta info ─────────────────────────────────────────────────────────
        RowLayout {
            spacing: 12

            Text {
                text: "Matched rule: "
                color: "#949ba4"
                font.pixelSize: 12
            }
            Text {
                text: AppController.matchedRuleName || "(private fallback)"
                color: AppController.matchedRuleName ? "#dbdee1" : "#949ba4"
                font.pixelSize: 12
                font.italic: !AppController.matchedRuleName
            }
        }

        RowLayout {
            spacing: 12

            Text {
                text: "Last update: "
                color: "#949ba4"
                font.pixelSize: 12
            }
            Text {
                text: AppController.lastUpdateTime || "—"
                color: "#dbdee1"
                font.pixelSize: 12
            }
        }

        // ── Publish button ────────────────────────────────────────────────────
        RowLayout {
            spacing: 12

            Button {
                text: AppController.discordConnected ? "⬆  Publish / Test" : "⬆  Preview only (not connected)"
                enabled: true
                onClicked: AppController.publishTest()

                background: Rectangle {
                    radius: 6
                    color: AppController.discordConnected
                           ? (parent.hovered ? "#4752c4" : "#5865f2")
                           : (parent.hovered ? "#3c3f45" : "#2b2d31")
                    border.color: AppController.discordConnected ? "transparent" : "#5865f2"
                    border.width: AppController.discordConnected ? 0 : 1
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment:   Text.AlignVCenter
                }
                implicitWidth: 220
                implicitHeight: 38
            }
        }

        // ── Private fallback notice ───────────────────────────────────────────
        Rectangle {
            visible: AppController.isPrivateFallback
            Layout.fillWidth: true
            radius: 6
            color: "#faa81a22"
            implicitHeight: privacyNote.implicitHeight + 16

            Text {
                id: privacyNote
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
                text: "⚠ Private fallback is active. No presence details are being sent to Discord."
                color: "#faa81a"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        Item { Layout.fillHeight: true }
    }
}

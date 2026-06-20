// CapturePage.qml — Capture the current focused window and seed a new rule from it.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    readonly property string pageId: "CapturePage.qml"
    background: Rectangle { color: "#313338" }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 16

        Text {
            text: "Capture Window"
            color: "#dbdee1"
            font.pixelSize: 20
            font.bold: true
        }

        Text {
            text: "Snapshot the currently focused window and use it as a starting point for a rule."
            color: "#949ba4"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Button {
            text: "📸  Capture Current Window"
            onClicked: {
                AppController.captureCurrentWindow()
                captured = true
            }
            property bool captured: false

            background: Rectangle {
                radius: 6
                color: parent.hovered ? "#4752c4" : "#5865f2"
            }
            contentItem: Text {
                text: parent.text
                color: "white"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment:   Text.AlignVCenter
            }
            implicitWidth: 220
            implicitHeight: 40
        }

        // ── Captured fields ───────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            radius: 8
            color: "#2b2d31"
            implicitHeight: captureLayout.implicitHeight + 32
            visible: AppController.currentProcessName !== ""

            ColumnLayout {
                id: captureLayout
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
                spacing: 8

                Text {
                    text: "DETECTED"
                    color: "#949ba4"
                    font.pixelSize: 11
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 1
                }

                CaptureRow { label: "Process";     value: AppController.currentProcessName }
                CaptureRow { label: "Window Title";value: AppController.currentWindowTitle }
                CaptureRow { label: "Window Class";value: AppController.currentWindowClass }
                CaptureRow { label: "Exe Path";    value: AppController.currentExePath;    mono: true }
            }
        }

        // ── "Create rule from this" CTA ───────────────────────────────────────
        Button {
            visible: AppController.currentProcessName !== ""
            text: "➕  Create Rule from This Window"
            onClicked: {
                // TODO: Pre-populate RulesPage editor with the captured values.
                // For now navigate to the Rules page.
                StackView.view.replace(null, Qt.resolvedUrl("RulesPage.qml"))
            }

            background: Rectangle {
                radius: 6
                color: parent.hovered ? "#2e5e3e" : "#23a55a"
            }
            contentItem: Text {
                text: parent.text
                color: "white"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment:   Text.AlignVCenter
            }
            implicitWidth: 260
            implicitHeight: 38
        }

        Item { Layout.fillHeight: true }
    }

    // ── Inline helper ─────────────────────────────────────────────────────────
    component CaptureRow: RowLayout {
        property string label: ""
        property string value: ""
        property bool   mono: false

        spacing: 8

        Text {
            text: label + ":"
            color: "#949ba4"
            font.pixelSize: 12
            Layout.minimumWidth: 90
        }
        Text {
            text: value || "—"
            color: value ? "#dbdee1" : "#4f5660"
            font.pixelSize: 13
            font.family: mono ? "Consolas, monospace" : ""
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}

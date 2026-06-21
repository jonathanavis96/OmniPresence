// LogsPage.qml — In-app view of the live presence timeline + app-icon backlog.
// Reads the two on-disk logs through AppController and auto-refreshes so you can
// watch what's publishing and spot misfires without opening files.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    readonly property string pageId: "LogsPage.qml"
    background: Rectangle { color: "#313338" }

    property string eventsText: ""
    property string coverageText: ""

    function refresh() {
        eventsText   = AppController.presenceEventsLog()
        coverageText = AppController.appCoverageLog()
    }

    Component.onCompleted: refresh()
    Timer { interval: 3000; running: true; repeat: true; onTriggered: root.refresh() }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Logs"; color: "#dbdee1"; font.pixelSize: 20; font.bold: true; Layout.fillWidth: true }
            Button {
                text: "↻ Refresh"
                onClicked: root.refresh()
                background: Rectangle { radius: 6; color: parent.hovered ? "#4752c4" : "#5865f2" }
                contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                implicitHeight: 32; implicitWidth: 100
            }
        }

        // ── Presence timeline ─────────────────────────────────────────────────
        Text {
            text: "Presence timeline — what published, and why (RuneScape lines show the raw signal trail)."
            color: "#949ba4"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        LogBox { Layout.fillWidth: true; Layout.preferredHeight: 0; Layout.fillHeight: true; logText: root.eventsText }

        // ── App coverage / icon backlog ───────────────────────────────────────
        Text {
            text: "Icon backlog — every app you've focused and whether it resolved to an icon. 'NO ICON' = candidate for a custom rule."
            color: "#949ba4"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        LogBox { Layout.fillWidth: true; Layout.preferredHeight: 160; logText: root.coverageText }
    }

    // ── Inline monospace, read-only, scrollable log box ───────────────────────
    component LogBox: Rectangle {
        property alias logText: ta.text
        radius: 8
        color: "#1e1f22"
        border.color: "#2b2d31"

        ScrollView {
            anchors { fill: parent; margins: 8 }
            clip: true
            TextArea {
                id: ta
                readOnly: true
                wrapMode: TextArea.NoWrap
                color: "#c7ccd1"
                font.family: "Cascadia Mono, Consolas, monospace"
                font.pixelSize: 12
                selectByMouse: true
                background: Item {}
            }
        }
    }
}

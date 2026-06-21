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
        // Only reassign when the text actually changed — reassigning on every
        // tick forces the TextArea to relayout and snaps the scrollbar back to
        // the top, which made the log impossible to scroll through.
        var e = AppController.presenceEventsLog()
        if (e !== eventsText) eventsText = e
        var c = AppController.appCoverageLog()
        if (c !== coverageText) coverageText = c
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
        LogBox { Layout.fillWidth: true; Layout.preferredHeight: 0; Layout.fillHeight: true; Layout.preferredWidth: 3; logText: root.eventsText }

        // ── App coverage / icon backlog ───────────────────────────────────────
        Text {
            text: "Icon backlog — every app you've focused and whether it resolved to an icon. 'NO ICON' = candidate for a custom rule. (Self-heals: once an app gets an icon its line flips to ICON ✓ next time it's focused.)"
            color: "#949ba4"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        LogBox { Layout.fillWidth: true; Layout.preferredHeight: 220; logText: root.coverageText }
    }

    // ── Inline monospace, read-only, scrollable log box ───────────────────────
    // Tail-follows: jumps to the newest line on update ONLY while you're parked
    // at the bottom — scroll up and it leaves your position alone so you can read.
    component LogBox: Rectangle {
        property alias logText: ta.text
        radius: 8
        color: "#1e1f22"
        border.color: "#2b2d31"

        ScrollView {
            id: sv
            anchors { fill: parent; margins: 8 }
            clip: true
            // "stick" = keep pinned to the newest line. Stays true until the USER
            // scrolls up; a content/relayout reset (which momentarily yanks contentY
            // to 0) must NOT flip it off, or the log fights you on every refresh.
            property bool stick: true

            function toBottom() {
                var f = sv.contentItem
                if (f) f.contentY = Math.max(0, f.contentHeight - f.height)
            }

            TextArea {
                id: ta
                readOnly: true
                wrapMode: TextArea.NoWrap
                color: "#c7ccd1"
                font.family: "Cascadia Mono, Consolas, monospace"
                font.pixelSize: 12
                selectByMouse: true
                background: Item {}
                onTextChanged: if (sv.stick) Qt.callLater(sv.toBottom)
            }

            Connections {
                target: sv.contentItem
                // Re-pin AFTER the content height settles — fixes the old bug where
                // toBottom() ran against a stale (small) contentHeight and landed near
                // the top.
                function onContentHeightChanged() {
                    if (sv.stick) Qt.callLater(sv.toBottom)
                }
                // Only a genuine user drag/flick updates whether we're following;
                // programmatic relayout (moving/dragging/flicking all false) is ignored.
                function onContentYChanged() {
                    var f = sv.contentItem
                    if (f.moving || f.dragging || f.flicking)
                        sv.stick = (f.contentY >= f.contentHeight - f.height - 8)
                }
            }
        }
    }
}

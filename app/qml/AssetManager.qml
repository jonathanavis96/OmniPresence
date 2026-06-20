// AssetManager.qml — Assign Discord image keys to known status types.
//
// NOTE: Image assets must be uploaded to the Discord Developer Portal under
// your application's "Rich Presence → Art Assets" section.  They are NOT
// bundled with this application.  The key names entered here must exactly
// match the keys you uploaded to the portal.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    readonly property string pageId: "AssetManager.qml"
    background: Rectangle { color: "#313338" }

    // Known status slots — extend as more activity types are added.
    // TODO: Wire to ConfigStore so keys persist across sessions.
    ListModel {
        id: assetModel
        ListElement { slotName: "osrs";       slotLabel: "Old School RuneScape";  imageKey: "osrs";       imageText: "Playing OSRS" }
        ListElement { slotName: "code";       slotLabel: "VS Code / IDE";         imageKey: "vscode";     imageText: "Writing code" }
        ListElement { slotName: "terminal";   slotLabel: "Terminal";              imageKey: "terminal";   imageText: "In the terminal" }
        ListElement { slotName: "youtube";    slotLabel: "YouTube";               imageKey: "youtube";    imageText: "Watching YouTube" }
        ListElement { slotName: "reddit";     slotLabel: "Reddit";                imageKey: "reddit";     imageText: "Browsing Reddit" }
        ListElement { slotName: "pihole";     slotLabel: "Pi-hole Dashboard";     imageKey: "pihole";     imageText: "Managing Pi-hole" }
        ListElement { slotName: "dashboard";  slotLabel: "Generic Dashboard";     imageKey: "dashboard";  imageText: "On a dashboard" }
        ListElement { slotName: "discord";    slotLabel: "Discord";               imageKey: "discord";    imageText: "On Discord" }
        ListElement { slotName: "fallback";   slotLabel: "Private / Fallback";    imageKey: "computer";   imageText: "Working privately" }
    }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 16

        Text {
            text: "Asset Manager"
            color: "#dbdee1"
            font.pixelSize: 20
            font.bold: true
        }

        // Portal link notice
        Rectangle {
            Layout.fillWidth: true
            radius: 8
            color: "#5865f222"
            implicitHeight: portalNote.implicitHeight + 20

            Text {
                id: portalNote
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                text: "⚙  Upload art assets at discordapp.com/developers/applications → your app → Rich Presence → Art Assets. "
                    + "Image keys are case-sensitive and must match exactly what you enter below."
                color: "#949ba4"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        // ── Asset table header ────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 0

            Text { text: "Slot";        color: "#949ba4"; font.pixelSize: 11; font.capitalization: Font.AllUppercase; Layout.minimumWidth: 160; font.letterSpacing: 1 }
            Text { text: "Image Key";   color: "#949ba4"; font.pixelSize: 11; font.capitalization: Font.AllUppercase; Layout.fillWidth: true;   font.letterSpacing: 1 }
            Text { text: "Hover Text";  color: "#949ba4"; font.pixelSize: 11; font.capitalization: Font.AllUppercase; Layout.fillWidth: true;   font.letterSpacing: 1 }
        }

        Rectangle { height: 1; Layout.fillWidth: true; color: "#3f4147" }

        // ── Asset rows ────────────────────────────────────────────────────────
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: assetModel
            clip: true
            spacing: 4

            delegate: RowLayout {
                width: ListView.view.width
                spacing: 0

                // Slot label
                Text {
                    text: model.slotLabel
                    color: "#dbdee1"
                    font.pixelSize: 13
                    Layout.minimumWidth: 160
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                // Image key field
                TextField {
                    text: model.imageKey
                    font.family: "Consolas, monospace"
                    font.pixelSize: 13
                    placeholderText: "key_name"
                    Layout.fillWidth: true
                    leftPadding: 8
                    background: Rectangle { radius: 4; color: "#1e1f22" }
                    color: "#dbdee1"
                    onTextChanged: assetModel.setProperty(index, "imageKey", text)
                }

                Item { width: 8 }

                // Hover text field
                TextField {
                    text: model.imageText
                    font.pixelSize: 13
                    placeholderText: "Shown on hover"
                    Layout.fillWidth: true
                    leftPadding: 8
                    background: Rectangle { radius: 4; color: "#1e1f22" }
                    color: "#dbdee1"
                    onTextChanged: assetModel.setProperty(index, "imageText", text)
                }
            }
        }

        // ── Save button ───────────────────────────────────────────────────────
        Button {
            text: "Save Asset Keys"
            onClicked: AppController.saveConfig()

            background: Rectangle {
                radius: 6
                color: parent.hovered ? "#4752c4" : "#5865f2"
            }
            contentItem: Text {
                text: parent.text
                color: "white"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment:   Text.AlignVCenter
            }
            implicitWidth: 160
            implicitHeight: 36
        }

        Item { height: 12 }
    }
}

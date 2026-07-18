// Main.qml — Root application window with sidebar navigation.
// Bound to AppController via the QML context property set in main.cpp.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OmniPresence

ApplicationWindow {
    id: root
    title: qsTr("OmniPresence")
    width: 900
    height: 600
    minimumWidth: 700
    minimumHeight: 450
    visible: true

    // ── Colour palette ────────────────────────────────────────────────────────
    readonly property color colorBg:       "#1e1f22"
    readonly property color colorSidebar:  "#2b2d31"
    readonly property color colorPanel:    "#313338"
    readonly property color colorAccent:   "#5865f2"
    readonly property color colorText:     "#dbdee1"
    readonly property color colorMuted:    "#949ba4"
    readonly property color colorDanger:   "#ed4245"
    readonly property color colorSuccess:  "#23a55a"

    color: colorBg

    // ── Layout ────────────────────────────────────────────────────────────────
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar
        Rectangle {
            Layout.fillHeight: true
            width: 200
            color: root.colorSidebar

            ColumnLayout {
                anchors { top: parent.top; left: parent.left; right: parent.right; margins: 8 }
                spacing: 2

                // App header
                Item { height: 16 }
                Text {
                    text: "OmniPresence"
                    color: root.colorText
                    font.pixelSize: 15
                    font.bold: true
                    leftPadding: 8
                }
                Text {
                    text: AppController.discordConnected ? "● Connected" : "○ Disconnected"
                    color: AppController.discordConnected ? root.colorSuccess : root.colorMuted
                    font.pixelSize: 11
                    leftPadding: 8
                }
                Item { height: 8 }

                Repeater {
                    model: [
                        { label: "Dashboard",    page: "Dashboard.qml",    comp: pgDashboard },
                        { label: "Capture",      page: "CapturePage.qml",  comp: pgCapture   },
                        { label: "Rules",        page: "RulesPage.qml",    comp: pgRules     },
                        { label: "Custom",       page: "CustomPage.qml",   comp: pgCustom    },
                        { label: "Preview",      page: "PreviewPage.qml",  comp: pgPreview   },
                        { label: "Logs",         page: "LogsPage.qml",     comp: pgLogs      },
                        { label: "Privacy",      page: "PrivacyPage.qml",  comp: pgPrivacy   },
                    ]

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        height: 36
                        radius: 6
                        color: stackView.currentItem
                               && stackView.currentItem.pageId === modelData.page
                               ? Qt.rgba(88/255, 101/255, 242/255, 0.3)
                               : "transparent"

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            leftPadding: 12
                            text: modelData.label
                            color: root.colorText
                            font.pixelSize: 13
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: stackView.replace(null, modelData.comp)
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // Quick pause toggle at the bottom of the sidebar
                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    radius: 6
                    color: AppController.paused ? Qt.rgba(237/255, 66/255, 69/255, 0.25) : "transparent"

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        leftPadding: 12
                        text: AppController.paused ? "⏸ Resume" : "⏸ Pause"
                        color: AppController.paused ? root.colorDanger : root.colorMuted
                        font.pixelSize: 13
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: AppController.paused ? AppController.resume() : AppController.pause()
                    }
                }

                Item { height: 8 }
            }
        }

        // Main content area
        StackView {
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            initialItem: pgDashboard

            pushEnter:  Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
            pushExit:   Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 80  } }
            replaceEnter: pushEnter
            replaceExit:  pushExit

            Component.onCompleted: console.log("[OP] StackView initial currentItem =", currentItem)
        }
    }

    // Page components — referenced as sibling types from the OmniPresence module.
    // (Loading sibling pages by qrc URL did not instantiate them; type-based
    //  Components are the canonical qt_add_qml_module approach.)
    Component { id: pgDashboard; Dashboard {} }
    Component { id: pgCapture;   CapturePage {} }
    Component { id: pgRules;     RulesPage {} }
    Component { id: pgCustom;    CustomPage {} }
    Component { id: pgPreview;   PreviewPage {} }
    Component { id: pgLogs;      LogsPage {} }
    Component { id: pgPrivacy;   PrivacyPage {} }
}

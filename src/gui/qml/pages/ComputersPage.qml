import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

ScrollView {
    id: page

    contentWidth: availableWidth
    clip: true

    property int selected: 0
    readonly property var computer: App.computers.length > selected ? App.computers[selected] : null
    readonly property int cell: computer ? App.layout.cellOf(computer.name) : -1

    ColumnLayout {
        x: 24
        y: 20
        width: page.availableWidth - 48
        spacing: 14

        ColumnLayout {
            spacing: 2
            Label { text: qsTr("Computers"); font.pixelSize: Theme.titleFontSize; font.weight: Font.DemiBold; color: Theme.text }
            Label {
                Layout.fillWidth: true
                text: qsTr("Every computer in the layout, this one included. What the server holds is editable here; what a computer holds itself is editable on that computer.")
                color: Theme.muted
                wrapMode: Text.WordWrap
            }
        }

        // The tabs: name and a state dot, one row, scrolling sideways when needed.
        Flickable {
            Layout.fillWidth: true
            implicitHeight: tabs.implicitHeight
            contentWidth: tabs.implicitWidth
            clip: true
            RowLayout {
                id: tabs
                spacing: 4
                Repeater {
                    model: App.computers
                    delegate: Button {
                        required property int index
                        required property var modelData
                        flat: page.selected !== index
                        highlighted: page.selected === index
                        onClicked: page.selected = index
                        contentItem: RowLayout {
                            spacing: 8
                            Rectangle { width: 8; height: 8; radius: 4; color: modelData.stateTone === "success" ? Theme.success : modelData.stateTone === "warning" ? Theme.warning : Theme.offline }
                            Label { text: modelData.name; color: page.selected === index ? Theme.onAccent : Theme.text; font.weight: Font.Medium }
                        }
                    }
                }
            }
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            visible: !page.computer
            title: qsTr("No computers yet")
            description: App.role === App.Server
                         ? qsTr("Add computers on the Layout page, or start the server and place them when they ask to join.")
                         : qsTr("Enter the server's address below or let discovery find it.")
        }

        Card {
            Layout.fillWidth: true
            visible: page.computer !== null
            padding: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Label { text: page.computer ? page.computer.name : ""; font.pixelSize: 16; font.weight: Font.DemiBold; color: Theme.text }
                StatusPill { text: page.computer && page.computer.isSelf ? (App.role === App.Server ? qsTr("this computer · server") : qsTr("this computer · client")) : (App.role === App.Server ? qsTr("client") : qsTr("server")); tone: "neutral"; showDot: false }
                StatusPill { text: page.computer ? page.computer.state : ""; tone: page.computer ? page.computer.stateTone : "neutral" }
                Item { Layout.fillWidth: true }
                Label { text: page.computer && page.computer.position.length > 0 ? page.computer.position : ""; color: Theme.muted }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 16
                rowSpacing: 12

                // ---- held by the server ---------------------------------
                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    visible: App.role === App.Server
                    padding: 14
                    color: Theme.surface
                    RowLayout {
                        Layout.fillWidth: true
                        SectionTitle { text: qsTr("Held by the server") }
                        Item { Layout.fillWidth: true }
                        StatusPill { text: qsTr("editable here"); tone: "success"; showDot: false }
                    }
                    ScreenSettingsPanel {
                        Layout.fillWidth: true
                        visible: page.cell >= 0
                        cell: page.cell
                        compact: true
                    }
                    Label {
                        visible: page.cell < 0
                        Layout.fillWidth: true
                        text: qsTr("%1 is not in the layout yet. Place it on the Layout page to configure it.").arg(page.computer ? page.computer.name : "")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                    }
                }

                // ---- on that computer -------------------------------------
                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    padding: 14
                    spacing: 0
                    color: page.computer && page.computer.isSelf ? Theme.surface : Theme.surfaceAlt

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 6
                        SectionTitle { text: page.computer ? qsTr("On %1").arg(page.computer.name) : "" }
                        Item { Layout.fillWidth: true }
                        StatusPill {
                            text: page.computer && page.computer.isSelf ? qsTr("this computer") : qsTr("read-only")
                            tone: page.computer && page.computer.isSelf ? "info" : "neutral"
                            showDot: false
                        }
                    }

                    // this computer: the local settings
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: page.computer && page.computer.isSelf
                        spacing: 0
                        SettingRow {
                            label: qsTr("Screen name")
                            divider: false
                            TextField { Layout.preferredWidth: 160; text: App.settings.screenName; font.family: App.monoFontFamily; onEditingFinished: App.settings.screenName = text }
                        }
                        SettingRow {
                            visible: App.role === App.Client
                            label: qsTr("Server to connect to")
                            hint: App.discovery.servers.length > 0 ? qsTr("Discovery found: %1. Leave empty to use it.").arg(App.discovery.servers.join(", ")) : qsTr("Host name or address. Leave empty to use what discovery finds.")
                            stacked: true
                            TextField { Layout.fillWidth: true; placeholderText: qsTr("server address"); text: App.settings.serverHostname; font.family: App.monoFontFamily; onEditingFinished: App.settings.serverHostname = text }
                        }
                        SettingRow {
                            label: qsTr("Find computers with Bonjour")
                            Switch { checked: App.settings.discovery; enabled: App.discovery.available; onToggled: { App.settings.discovery = checked; if (checked) App.discovery.restart(); else App.discovery.stop() } }
                        }
                        SettingRow {
                            visible: App.role === App.Client
                            label: qsTr("Scroll step")
                            hint: qsTr("Units per wheel notch; 120 is the default.")
                            NumberBox { from: 1; to: 10000; value: App.settings.scrollStep; onValueModified: App.settings.scrollStep = value }
                        }
                        SettingRow {
                            label: qsTr("Overlay hints")
                            hint: qsTr("Dimmed, cursor locked elsewhere.")
                            Switch { checked: App.settings.overlayHints; onToggled: App.settings.overlayHints = checked }
                        }
                        SettingRow {
                            label: qsTr("Log level")
                            ComboBox {
                                Layout.preferredWidth: 140
                                model: [qsTr("Error"), qsTr("Warning"), qsTr("Note"), qsTr("Info"), qsTr("Debug"), qsTr("Debug 1"), qsTr("Debug 2")]
                                currentIndex: App.settings.logLevel
                                onActivated: (index) => App.settings.logLevel = index
                            }
                        }
                    }

                    // another computer: nothing until phase C
                    Label {
                        visible: page.computer && !page.computer.isSelf
                        Layout.fillWidth: true
                        text: qsTr("Its own settings (name, server address, discovery, overlay hints, log level, its core process) are edited on that computer. They will appear here once computers share their settings with each other.")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }
}

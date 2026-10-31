import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

ScrollView {
    id: page

    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        x: 24
        y: 20
        width: page.availableWidth - 48
        spacing: 16

        // Inline requests instead of pop-ups.
        Repeater {
            model: App.requests
            delegate: Banner {
                required property var modelData
                request: modelData
            }
        }

        // First run: no wizard, just the two decisions and Start.
        Card {
            Layout.fillWidth: true
            visible: App.firstRun && App.runState !== App.Running
            color: Theme.accentSoft
            border.color: Theme.accent
            padding: 14

            RowLayout {
                spacing: 14
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: qsTr("Get started"); font.weight: Font.DemiBold; color: Theme.text }
                    Label {
                        Layout.fillWidth: true
                        text: App.role === App.Server
                              ? qsTr("This computer will share its keyboard and mouse. Press Start, then add the other computers on the Layout page, or let them find you and place them when they ask to join.")
                              : qsTr("This computer will be controlled by another one. Enter the server's address on the Computers page or let discovery find it, then press Start.")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                    }
                }
                Button { text: qsTr("Start"); highlighted: true; onClicked: App.start() }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 16
            rowSpacing: 16

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true

                SectionTitle { text: qsTr("Status") }

                RowLayout {
                    spacing: 10
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: App.runState === App.Failed ? Theme.danger
                             : App.runState === App.Running && App.connected ? Theme.success
                             : App.runState === App.Running ? Theme.accent
                             : Theme.offline
                    }
                    Label {
                        Layout.fillWidth: true
                        text: App.statusText
                        font.pixelSize: Theme.titleFontSize
                        font.weight: Font.DemiBold
                        color: Theme.text
                        elide: Text.ElideRight
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    InfoRow { label: qsTr("Screen name"); value: App.screenName; mono: true }
                    InfoRow {
                        label: App.role === App.Server ? qsTr("Listening on") : qsTr("Addresses")
                        value: (App.addresses.length > 0 ? App.addresses.join(", ") : qsTr("no network address"))
                               + (App.role === App.Server ? "  ·  " + qsTr("port %1").arg(App.settings.port) : "")
                        mono: true
                    }
                    InfoRow {
                        visible: App.role === App.Client
                        label: qsTr("Server")
                        value: App.serverAddress.length > 0 ? App.serverAddress : qsTr("none yet")
                        mono: true
                    }
                    RowLayout {
                        spacing: 12
                        Label { Layout.preferredWidth: 120; text: qsTr("Encryption"); color: Theme.muted }
                        StatusPill {
                            text: App.settings.cryptoEnabled ? qsTr("TLS on") : qsTr("TLS off")
                            tone: App.settings.cryptoEnabled ? "success" : "warning"
                        }
                        Label {
                            visible: App.settings.cryptoEnabled && App.identity.ready
                            text: App.identity.sha256Short
                            font.family: App.monoFontFamily
                            color: Theme.muted
                        }
                    }
                    InfoRow {
                        visible: App.role === App.Server
                        label: qsTr("Configuration")
                        value: App.settings.useExternalConfig
                               ? qsTr("from %1 · %2 screens").arg(App.settings.configFile).arg(App.layout.screenCount)
                               : qsTr("generated from the layout · %1 screens, %2 hotkeys").arg(App.layout.screenCount).arg(App.layout.hotkeys.length)
                    }
                    InfoRow {
                        label: qsTr("Cursor")
                        value: App.runState !== App.Running ? qsTr("—")
                             : App.cursorLockedElsewhere ? qsTr("locked to another screen")
                             : App.cursorLocked ? qsTr("locked to this screen")
                             : App.role === App.Server
                               ? (App.activeScreen.length > 0 ? qsTr("on %1").arg(App.activeScreen) : qsTr("on this computer"))
                               : (App.dimmed ? qsTr("elsewhere · this screen is dimmed") : qsTr("free"))
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 360

                RowLayout {
                    Layout.fillWidth: true
                    SectionTitle { text: qsTr("Layout") }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: App.role === App.Server ? qsTr("%1 screens").arg(App.layout.screenCount) : qsTr("set on the server")
                        color: Theme.muted
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 150
                    radius: Theme.radiusSmall
                    color: Theme.bg

                    LayoutCanvas {
                        anchors.fill: parent
                        anchors.margins: 6
                        padding: 6
                        maxScale: 0.06
                        showDetails: false
                        selectedIndex: -1
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: App.layout.screenCount === 0
                        text: App.role === App.Server ? qsTr("no computers placed yet") : qsTr("the server holds the layout")
                        color: Theme.muted
                        font.pixelSize: Theme.smallFontSize
                    }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            padding: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 8
                SectionTitle { text: qsTr("Computers") }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("Move the mouse off an edge to switch · Scroll Lock locks the cursor to a screen")
                    color: Theme.muted
                    elide: Text.ElideRight
                }
            }

            Repeater {
                model: App.computers

                delegate: RowLayout {
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.bottomMargin: 12
                    spacing: 12

                    Label {
                        text: modelData.name
                        font.family: App.monoFontFamily
                        font.weight: Font.Medium
                        color: Theme.text
                        Layout.preferredWidth: 170
                        elide: Text.ElideRight
                    }
                    Label { text: modelData.position; color: Theme.text; Layout.preferredWidth: 150; elide: Text.ElideRight }
                    StatusPill { text: modelData.state; tone: modelData.stateTone }
                    Item { Layout.fillWidth: true }
                    Label { text: modelData.since.length > 0 ? qsTr("since %1").arg(modelData.since) : ""; color: Theme.muted }
                    Label { text: modelData.address; font.family: App.monoFontFamily; color: Theme.muted }
                }
            }

            Label {
                visible: App.computers.length === 0
                Layout.margins: 16
                Layout.topMargin: 0
                text: qsTr("No computers yet")
                color: Theme.muted
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            SectionTitle { text: qsTr("What is running") }

            // five pieces: three and two, so the names stay readable
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 12
                rowSpacing: 12

                Repeater {
                    model: App.processes

                    delegate: Card {
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        padding: 12
                        spacing: 4
                        opacity: modelData.relevant ? 1 : 0.7

                        RowLayout {
                            spacing: 8
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: modelData.ok ? Theme.success : Theme.offline
                            }
                            Label {
                                text: modelData.name
                                font.weight: Font.DemiBold
                                color: Theme.text
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.detail
                            color: Theme.muted
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }
}

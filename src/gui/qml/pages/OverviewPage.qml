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

        // Inline request instead of a pop-up: a client wants to join.
        Card {
            Layout.fillWidth: true
            visible: App.pendingJoin.length > 0
            color: Theme.warningSoft
            border.color: Theme.warning
            padding: 12

            RowLayout {
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: qsTr("%1 wants to join").arg(App.pendingJoin)
                        font.weight: Font.DemiBold
                        color: Theme.text
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Discovered on the network. Choose where it sits next to %1, or ignore it.").arg(App.screenName)
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                    }
                }
                Button {
                    text: qsTr("Ignore")
                    onClicked: App.ignorePendingJoin()
                }
                Button {
                    text: qsTr("Place on layout")
                    highlighted: true
                }
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
                        color: App.runState === App.Running ? Theme.success : Theme.offline
                    }
                    Label {
                        text: App.statusText
                        font.pixelSize: Theme.titleFontSize
                        font.weight: Font.DemiBold
                        color: Theme.text
                    }
                }

                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8

                    Label { text: qsTr("Screen name"); color: Theme.muted }
                    Label { text: App.screenName; font.family: App.monoFontFamily; color: Theme.text }

                    Label { text: qsTr("Addresses"); color: Theme.muted }
                    Label {
                        text: App.addresses.length > 0 ? App.addresses.join(", ") : qsTr("none found")
                        font.family: App.monoFontFamily
                        color: Theme.text
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }

                    Label { text: qsTr("Role"); color: Theme.muted }
                    Label { text: App.role === App.Server ? qsTr("server · shares this keyboard and mouse") : qsTr("client · controlled by a server"); color: Theme.text }

                    Label { text: qsTr("Encryption"); color: Theme.muted }
                    StatusPill { text: qsTr("TLS on"); tone: "success" }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true

                RowLayout {
                    Layout.fillWidth: true
                    SectionTitle { text: qsTr("Layout") }
                    Item { Layout.fillWidth: true }
                    Label { text: qsTr("Edit layout"); color: Theme.accent }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    radius: Theme.radiusSmall
                    color: Theme.bg

                    Rectangle {
                        anchors.centerIn: parent
                        width: 150
                        height: 76
                        radius: Theme.radiusSmall
                        color: Theme.accentSoft
                        border.color: Theme.accent
                        border.width: 2

                        Column {
                            anchors.centerIn: parent
                            Label {
                                text: App.screenName
                                font.weight: Font.Bold
                                color: Theme.accent
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            Label {
                                text: App.role === App.Server ? qsTr("this computer · server") : qsTr("this computer · client")
                                font.pixelSize: 11
                                color: Theme.accent
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
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
                        Layout.preferredWidth: 160
                    }
                    Label { text: modelData.position; color: Theme.text; Layout.preferredWidth: 140 }
                    StatusPill { text: modelData.state; tone: modelData.stateTone }
                    Item { Layout.fillWidth: true }
                    Label { text: modelData.address; font.family: App.monoFontFamily; color: Theme.muted }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                SectionTitle { text: qsTr("What is running") }
                Item { Layout.fillWidth: true }
                Label { text: qsTr("All processes"); color: Theme.accent }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Repeater {
                    model: App.processes

                    delegate: Card {
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        padding: 12
                        spacing: 4

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

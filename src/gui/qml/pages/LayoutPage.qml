import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// The server's layout: a grid of cells, one tile per computer, dragged
// into place. Phase A of docs/ui-redesign.md §4.4 (per-display placement
// comes later).
Item {
    id: page

    property int selectedCell: App.layout.cellOf(App.screenName)
    readonly property int panelWidth: 300
    // as wide as the box allows, as tall as the names need: narrow tiles
    // get a taller shape so a wrapped name still fits
    readonly property int cellWidth: Math.max(80, Math.min(150, Math.floor((gridBox.width - 32 - (App.layout.columns - 1) * 8) / App.layout.columns)))
    readonly property int cellHeight: Math.round(cellWidth * (cellWidth < 120 ? 0.8 : 0.64))

    EmptyState {
        anchors.fill: parent
        visible: App.role === App.Client
        title: qsTr("The layout lives on the server")
        description: qsTr("This computer is a client: the server decides where every screen sits. Switch to the server role here to edit a layout of this computer's own.")
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 20
        visible: App.role === App.Server
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: qsTr("Screen layout"); font.pixelSize: Theme.titleFontSize; font.weight: Font.DemiBold; color: Theme.text }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Drag the computers to where they sit on your desk. Edges that touch become links. Changes apply when the server restarts.")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                    }
                }
                Button { text: qsTr("Add computer"); visible: !addRow.visible; onClicked: { addRow.visible = true; addField.forceActiveFocus() } }
                Button { text: qsTr("Apply and restart"); highlighted: true; enabled: App.runState === App.Running; onClicked: App.restart() }
            }

            RowLayout {
                id: addRow
                visible: false
                spacing: 8
                TextField {
                    id: addField
                    Layout.preferredWidth: 220
                    placeholderText: qsTr("Computer's screen name")
                    font.family: App.monoFontFamily
                    validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9._-]{0,255}/ }
                    onAccepted: addButton.clicked()
                }
                Button {
                    id: addButton
                    text: qsTr("Add")
                    highlighted: true
                    enabled: addField.text.length > 0
                    onClicked: {
                        if (App.layout.addScreenAnywhere(addField.text)) {
                            page.selectedCell = App.layout.cellOf(addField.text)
                            addField.text = ""
                            addRow.visible = false
                        }
                    }
                }
                Button { text: qsTr("Cancel"); flat: true; onClicked: { addRow.visible = false; addField.text = "" } }
                Label { text: qsTr("It lands on a free cell; drag it where it belongs."); color: Theme.muted }
            }

            // ---- the grid -------------------------------------------------------
            Rectangle {
                id: gridBox
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: grid.implicitHeight + 32
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                border.width: 1

                Grid {
                    id: grid
                    anchors.centerIn: parent
                    columns: App.layout.columns
                    spacing: 8

                    Repeater {
                        model: App.layout

                        delegate: Item {
                            id: cellItem
                            required property int index
                            required property string name
                            required property bool occupied
                            required property bool isServer
                            required property string state
                            required property string position

                            width: page.cellWidth
                            height: page.cellHeight
                            z: mouse.drag.active ? 10 : 0

                            DropArea {
                                id: drop
                                anchors.fill: parent
                                keys: ["screen"]
                                onDropped: (event) => {
                                    const from = event.source.cellIndex
                                    if (from !== cellItem.index && App.layout.moveScreen(from, cellItem.index)) {
                                        page.selectedCell = cellItem.index
                                    }
                                    event.accept(Qt.MoveAction)
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: Theme.radiusSmall
                                color: drop.containsDrag ? Theme.accentSoft : "transparent"
                                border.color: drop.containsDrag ? Theme.accent : Theme.borderSoft
                                border.width: 1
                            }

                            Rectangle {
                                id: tile
                                visible: cellItem.occupied
                                property int cellIndex: cellItem.index
                                x: 4
                                y: 4
                                width: cellItem.width - 8
                                height: cellItem.height - 8
                                radius: Theme.radiusSmall
                                color: cellItem.isServer ? Theme.accentSoft : Theme.surface
                                border.color: page.selectedCell === cellItem.index ? Theme.accent : cellItem.isServer ? Theme.accent : Theme.border
                                border.width: page.selectedCell === cellItem.index ? 2 : 1
                                opacity: mouse.drag.active ? 0.85 : 1

                                Drag.active: mouse.drag.active
                                Drag.keys: ["screen"]
                                Drag.hotSpot.x: width / 2
                                Drag.hotSpot.y: height / 2

                                Column {
                                    anchors.centerIn: parent
                                    width: parent.width - 12
                                    spacing: 2
                                    Label {
                                        width: parent.width
                                        text: cellItem.name
                                        font.weight: Font.DemiBold
                                        font.pixelSize: page.cellWidth < 120 ? Theme.smallFontSize : 14
                                        color: cellItem.isServer ? Theme.accent : Theme.text
                                        wrapMode: Text.Wrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                    Label {
                                        width: parent.width
                                        text: cellItem.isServer ? qsTr("this computer · server")
                                            : cellItem.state === "connected" ? qsTr("connected")
                                            : cellItem.state === "offline" ? qsTr("offline")
                                            : cellItem.position
                                        font.pixelSize: 11
                                        visible: page.cellWidth >= 100 || cellItem.state === "connected"
                                        color: cellItem.state === "connected" ? Theme.success : Theme.muted
                                        elide: Text.ElideRight
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }

                                MouseArea {
                                    id: mouse
                                    anchors.fill: parent
                                    drag.target: tile
                                    drag.threshold: 6
                                    onPressed: page.selectedCell = cellItem.index
                                    onReleased: {
                                        tile.Drag.drop()
                                        tile.x = 4
                                        tile.y = 4
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ---- found on the network, not placed yet ----------------------------
            Repeater {
                model: App.discovery.clients.filter(function(n) { return !App.layout.hasScreen(n) })
                delegate: Card {
                    required property string modelData
                    Layout.fillWidth: true
                    padding: 12
                    color: Theme.warningSoft
                    border.color: Theme.warning
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Label { text: modelData; font.family: App.monoFontFamily; font.weight: Font.DemiBold; color: Theme.text }
                        Label { Layout.fillWidth: true; text: qsTr("found on the network, not placed yet"); color: Theme.muted; elide: Text.ElideRight }
                        Button { text: qsTr("Left"); onClicked: App.placeJoin(modelData, LayoutModel.Left) }
                        Button { text: qsTr("Above"); onClicked: App.placeJoin(modelData, LayoutModel.Up) }
                        Button { text: qsTr("Below"); onClicked: App.placeJoin(modelData, LayoutModel.Down) }
                        Button { text: qsTr("Right"); highlighted: true; onClicked: App.placeJoin(modelData, LayoutModel.Right) }
                        Button { text: qsTr("Ignore"); flat: true; onClicked: App.ignoreJoin(modelData) }
                    }
                }
            }
        }

        // ---- the selected computer ------------------------------------------------
        Card {
            Layout.preferredWidth: page.panelWidth
            Layout.fillHeight: true
            padding: 14

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 12

                    Label {
                        visible: page.selectedCell < 0
                        Layout.fillWidth: true
                        text: qsTr("Select a computer to see its settings.")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                    }

                    ScreenSettingsPanel {
                        id: panel
                        Layout.fillWidth: true
                        visible: page.selectedCell >= 0 && valid
                        cell: page.selectedCell
                        compact: true
                    }

                    Button {
                        visible: panel.visible
                        text: qsTr("Remove from layout")
                        enabled: panel.valid && !panel.s.isServer
                        onClicked: { App.layout.removeScreen(page.selectedCell); page.selectedCell = -1 }
                    }
                }
            }
        }
    }
}

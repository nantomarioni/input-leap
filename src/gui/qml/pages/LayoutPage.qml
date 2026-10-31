import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// The server's layout: the computers' desktops to scale, dragged into
// place; touching edges become links (docs/ui-redesign.md §4.4, phase B).
Item {
    id: page

    property int selectedIndex: App.layout.indexOf(App.screenName)
    readonly property int panelWidth: 300
    // the server runs a configuration file: shown as the core reads it, edited there
    readonly property bool fromFile: App.role === App.Server && App.layout.isMirror

    EmptyState {
        anchors.fill: parent
        visible: App.role === App.Client && !App.mirrorAvailable
        title: qsTr("The layout lives on the server")
        description: qsTr("This computer is a client: the server decides where every screen sits. It shows here, read-only, once the server shares its settings (Settings → Share settings) and this computer trusts it. Switch to the server role to edit a layout of this computer's own.")
    }

    // ---- the mirror: the server's layout as it shares it ---------------------
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 20
        visible: App.role === App.Client && App.mirrorAvailable
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
                    text: qsTr("As the server %1 holds it. Edit it there; it refreshes here as it changes.").arg(App.sync.serverName)
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                }
            }
            StatusPill { text: qsTr("mirrored from %1 · read-only").arg(App.sync.serverName); tone: "info"; showDot: false }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            clip: true
            LayoutCanvas {
                anchors.fill: parent
                anchors.margins: 8
                model: App.mirrorLayout
                interactive: false
                selectedIndex: App.mirrorLayout.indexOf(App.screenName)
            }
        }
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
                        text: page.fromFile
                              ? qsTr("As read from %1. Edit that file (it reloads here when saved), or choose \"Generate from the layout\" on the Server page to arrange it here.").arg(App.settings.configFile)
                              : qsTr("Drag the computers to where they sit on your desk; edges that touch become links. Each computer is drawn to the size of its displays. Changes apply when the server restarts.")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                    }
                }
                StatusPill { visible: page.fromFile; text: qsTr("from a file · read-only"); tone: "info"; showDot: false }
                Button { text: qsTr("Add computer"); visible: !addRow.visible && !page.fromFile; onClicked: { addRow.visible = true; addField.forceActiveFocus() } }
                Button { text: qsTr("Apply and restart"); visible: !page.fromFile; highlighted: true; enabled: App.runState === App.Running; onClicked: App.restart() }
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
                    Accessible.name: qsTr("New computer's screen name")
                    onAccepted: addButton.clicked()
                    Keys.onEscapePressed: { addRow.visible = false; addField.text = "" }
                }
                Button {
                    id: addButton
                    text: qsTr("Add")
                    highlighted: true
                    enabled: addField.text.length > 0
                    onClicked: {
                        if (App.layout.addScreenAnywhere(addField.text)) {
                            page.selectedIndex = App.layout.indexOf(addField.text)
                            addField.text = ""
                            addRow.visible = false
                        }
                    }
                }
                Button { text: qsTr("Cancel"); flat: true; onClicked: { addRow.visible = false; addField.text = "" } }
                Label { text: qsTr("It lands at the right end; drag it where it belongs."); color: Theme.muted }
            }

            // ---- the desktops ----------------------------------------------------
            Rectangle {
                id: canvasBox
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 220
                radius: Theme.radius
                color: Theme.surface
                border.color: rejectFlash.running ? Theme.danger : Theme.border
                border.width: 1
                clip: true

                LayoutCanvas {
                    id: canvas
                    anchors.fill: parent
                    anchors.margins: 8
                    anchors.bottomMargin: 28
                    interactive: !page.fromFile
                    selectedIndex: page.selectedIndex
                    onSelected: (index) => page.selectedIndex = index
                    onRejected: rejectFlash.restart()
                }

                Timer { id: rejectFlash; interval: 350 }

                Label {
                    anchors.centerIn: parent
                    visible: App.layout.screenCount === 0
                    text: qsTr("No computers yet. Start the server and place the ones that ask to join, or add one.")
                    color: Theme.muted
                    width: parent.width - 48
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Label {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 10
                    visible: App.layout.screenCount > 0
                    text: qsTr("1 px here = %1 px on the desks · Tab reaches a computer, arrows nudge it, Delete removes it").arg(Math.round(1 / canvas.pxScale))
                    font.pixelSize: 11
                    color: Theme.muted
                    elide: Text.ElideRight
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
                        visible: page.selectedIndex < 0
                        Layout.fillWidth: true
                        text: qsTr("Select a computer to see its settings.")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                    }

                    ScreenSettingsPanel {
                        id: panel
                        Layout.fillWidth: true
                        enabled: !page.fromFile
                        visible: page.selectedIndex >= 0 && valid
                        screenIndex: page.selectedIndex
                        compact: true
                    }

                    Button {
                        visible: panel.visible && !page.fromFile
                        text: qsTr("Remove from layout")
                        enabled: panel.valid && !panel.s.isServer
                        onClicked: { App.layout.removeScreen(page.selectedIndex); page.selectedIndex = -1 }
                    }
                }
            }
        }
    }
}

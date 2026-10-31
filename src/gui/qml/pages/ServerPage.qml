import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

Item {
    id: page

    // On a client the server's configuration is mirrored from the server
    // (phase C); until then the page says so.
    EmptyState {
        anchors.fill: parent
        visible: App.role === App.Client
        title: qsTr("The server's configuration lives on the server")
        description: qsTr("This computer is a client. The server's settings will show here, read-only, once it is connected and shares them; edit them on the server itself. Settings for this computer are on the Computers page.")
    }

    ScrollView {
        anchors.fill: parent
        visible: App.role === App.Server
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 24
            y: 20
            width: page.width - 48
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: qsTr("Server"); font.pixelSize: Theme.titleFontSize; font.weight: Font.DemiBold; color: Theme.text }
                    RowLayout {
                        spacing: 8
                        Label { text: qsTr("The server's configuration."); color: Theme.muted }
                        StatusPill { text: qsTr("This computer is the server"); tone: "success"; showDot: false }
                        Label { text: qsTr("Changes apply on the next restart."); color: Theme.muted }
                    }
                }
                Button { text: qsTr("Apply and restart"); highlighted: true; enabled: App.runState === App.Running; onClicked: App.restart() }
            }

            Card {
                Layout.fillWidth: true
                padding: 14
                spacing: 6
                SectionTitle { text: qsTr("Configuration source") }
                RadioButton {
                    text: qsTr("Generate from the layout and the options below (recommended)")
                    checked: !App.settings.useExternalConfig
                    onToggled: if (checked) App.settings.useExternalConfig = false
                }
                RowLayout {
                    spacing: 8
                    RadioButton {
                        text: qsTr("Use a configuration file")
                        checked: App.settings.useExternalConfig
                        onToggled: if (checked) App.settings.useExternalConfig = true
                    }
                    TextField {
                        Layout.preferredWidth: 360
                        enabled: App.settings.useExternalConfig
                        text: App.settings.configFile
                        font.family: App.monoFontFamily
                        onEditingFinished: App.settings.configFile = text
                    }
                    Button {
                        text: qsTr("Browse…")
                        enabled: App.settings.useExternalConfig
                        onClicked: { var f = App.pickFile(qsTr("Choose the server configuration file"), App.settings.configFile, false); if (f.length > 0) App.settings.configFile = f }
                    }
                }
                Label {
                    visible: !App.settings.useExternalConfig
                    text: qsTr("Written to %1 on every start.").arg(App.generatedConfigPath)
                    color: Theme.muted
                    font.pixelSize: Theme.smallFontSize
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 14
                rowSpacing: 14

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    padding: 14
                    spacing: 0
                    SectionTitle { text: qsTr("Listening"); Layout.bottomMargin: 6 }
                    SettingRow {
                        label: qsTr("Address")
                        hint: qsTr("Empty listens on every interface.")
                        divider: false
                        TextField {
                            Layout.preferredWidth: 170
                            placeholderText: qsTr("all interfaces")
                            text: App.settings.networkInterface
                            font.family: App.monoFontFamily
                            onEditingFinished: App.settings.networkInterface = text
                        }
                    }
                    SettingRow {
                        label: qsTr("Port")
                        NumberBox { from: 1; to: 65535; value: App.settings.port; onValueModified: App.settings.port = value }
                    }
                    SettingRow {
                        label: qsTr("TLS encryption")
                        hint: qsTr("Fingerprints on the Security page.")
                        Switch { checked: App.settings.cryptoEnabled; onToggled: App.settings.cryptoEnabled = checked }
                    }
                    SettingRow {
                        label: qsTr("Require client certificates")
                        Switch { checked: App.settings.requireClientCertificate; onToggled: App.settings.requireClientCertificate = checked }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    padding: 14
                    spacing: 0
                    SectionTitle { text: qsTr("Switching screens"); Layout.bottomMargin: 6 }
                    SettingRow {
                        label: qsTr("Switch after waiting at the edge")
                        divider: false
                        NumberBox { from: 0; to: 10000; stepSize: 50; value: App.layout.switchDelay; onValueModified: App.layout.switchDelay = value }
                        Label { text: qsTr("ms"); color: Theme.muted }
                        Switch { checked: App.layout.hasSwitchDelay; onToggled: App.layout.hasSwitchDelay = checked }
                    }
                    SettingRow {
                        label: qsTr("Switch on double tap within")
                        NumberBox { from: 0; to: 10000; stepSize: 50; value: App.layout.switchDoubleTap; onValueModified: App.layout.switchDoubleTap = value }
                        Label { text: qsTr("ms"); color: Theme.muted }
                        Switch { checked: App.layout.hasSwitchDoubleTap; onToggled: App.layout.hasSwitchDoubleTap = checked }
                    }
                    SettingRow {
                        label: qsTr("Dead corners")
                        hint: qsTr("Default for every screen.")
                        stacked: true
                        Flow {
                            Layout.fillWidth: true
                            spacing: 4
                            Repeater {
                                model: [qsTr("Top-left"), qsTr("Top-right"), qsTr("Bottom-left"), qsTr("Bottom-right")]
                                delegate: CheckBox {
                                    required property int index
                                    required property string modelData
                                    text: modelData
                                    checked: App.layout.switchCorners[index]
                                    onToggled: App.layout.setSwitchCorner(index, checked)
                                }
                            }
                        }
                    }
                    SettingRow {
                        label: qsTr("Corner size")
                        NumberBox { from: 0; to: 1000; value: App.layout.switchCornerSize; onValueModified: App.layout.switchCornerSize = value }
                        Label { text: qsTr("px"); color: Theme.muted }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    padding: 14
                    spacing: 0
                    SectionTitle { text: qsTr("Behaviour"); Layout.bottomMargin: 6 }
                    SettingRow {
                        label: qsTr("Share the clipboard")
                        divider: false
                        Label { text: qsTr("up to"); color: Theme.muted }
                        NumberBox { from: 1; to: 10000; value: App.layout.clipboardSharingSizeMb; onValueModified: App.layout.clipboardSharingSizeMb = value }
                        Label { text: qsTr("MB"); color: Theme.muted }
                        Switch { checked: App.layout.clipboardSharing; onToggled: App.layout.clipboardSharing = checked }
                    }
                    SettingRow {
                        label: qsTr("Relative mouse moves")
                        hint: qsTr("For games and apps that grab the mouse.")
                        Switch { checked: App.layout.relativeMouseMoves; onToggled: App.layout.relativeMouseMoves = checked }
                    }
                    SettingRow {
                        label: qsTr("Synchronise screen savers")
                        Switch { checked: App.layout.screenSaverSync; onToggled: App.layout.screenSaverSync = checked }
                    }
                    SettingRow {
                        label: qsTr("Keep the foreground window")
                        hint: qsTr("Windows servers only.")
                        Switch { checked: App.layout.win32KeepForeground; onToggled: App.layout.win32KeepForeground = checked }
                    }
                    SettingRow {
                        label: qsTr("Check clients every")
                        NumberBox { from: 1000; to: 60000; stepSize: 500; value: App.layout.heartbeat; onValueModified: App.layout.heartbeat = value }
                        Label { text: qsTr("ms"); color: Theme.muted }
                        Switch { checked: App.layout.hasHeartbeat; onToggled: App.layout.hasHeartbeat = checked }
                    }
                    SettingRow {
                        label: qsTr("Drag & drop file transfer")
                        Switch { checked: App.layout.enableDragAndDrop; onToggled: App.layout.enableDragAndDrop = checked }
                    }
                    SettingRow {
                        label: qsTr("Ignore computers found by discovery")
                        hint: qsTr("No join requests; add computers by hand.")
                        Switch { checked: App.layout.ignoreAutoConfigClient; onToggled: App.layout.ignoreAutoConfigClient = checked }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    padding: 14
                    spacing: 0
                    SectionTitle { text: qsTr("Inactive screens"); Layout.bottomMargin: 6 }
                    SettingRow {
                        label: qsTr("Dim the screens the cursor is not on")
                        divider: false
                        Switch { checked: App.layout.dimmingEnabled; onToggled: App.layout.dimmingEnabled = checked }
                    }
                    SettingRow {
                        label: qsTr("Dimmed brightness")
                        Slider { Layout.preferredWidth: 140; from: 0; to: 100; stepSize: 5; value: App.layout.dimmingPercentage; enabled: App.layout.dimmingEnabled; onMoved: App.layout.dimmingPercentage = value }
                        Label { text: App.layout.dimmingPercentage + " %"; font.family: App.monoFontFamily; color: Theme.text; Layout.preferredWidth: 40 }
                    }
                    SettingRow {
                        label: qsTr("Clients wake on local input")
                        Label { text: qsTr("always on"); color: Theme.muted }
                    }
                    SettingRow {
                        label: qsTr("Overlay hints")
                        hint: qsTr("Each computer decides for itself, in its Settings.")
                        Label { text: qsTr("per computer"); color: Theme.muted }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                padding: 14
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    SectionTitle { text: qsTr("Hotkeys & actions") }
                    Item { Layout.fillWidth: true }
                    Button { text: qsTr("Add hotkey"); visible: !newHotkey.visible; onClicked: newHotkey.visible = true }
                }

                HotkeyEditor {
                    id: newHotkey
                    Layout.fillWidth: true
                    visible: false
                    hotkeyIndex: -1
                    onDone: visible = false
                }

                Label {
                    visible: App.layout.hotkeys.length === 0 && !newHotkey.visible
                    text: qsTr("No hotkeys yet. A hotkey runs one or more actions: switch to a screen, lock the cursor, or send a key combination.")
                    color: Theme.muted
                }

                Repeater {
                    model: App.layout.hotkeys
                    delegate: ColumnLayout {
                        id: hotkeyRow
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 4

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSoft }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Label { text: hotkeyRow.modelData.key; font.family: App.monoFontFamily; font.weight: Font.DemiBold; color: Theme.text; Layout.preferredWidth: 180 }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Repeater {
                                    model: hotkeyRow.modelData.actions
                                    delegate: RowLayout {
                                        required property int index
                                        required property string modelData
                                        spacing: 8
                                        Label { Layout.fillWidth: true; text: modelData; font.family: App.monoFontFamily; color: Theme.text; elide: Text.ElideRight }
                                        Button { text: qsTr("Remove"); flat: true; onClicked: { App.layout.removeAction(hotkeyRow.index, index); App.applyLayout() } }
                                    }
                                }
                                Label { visible: hotkeyRow.modelData.actions.length === 0; text: qsTr("no actions yet"); color: Theme.muted }
                            }
                            Button { text: qsTr("Add action"); flat: true; onClicked: actionEditor.visible = true }
                            Button { text: qsTr("Remove hotkey"); flat: true; onClicked: { App.layout.removeHotkey(hotkeyRow.index); App.applyLayout() } }
                        }
                        HotkeyEditor {
                            id: actionEditor
                            Layout.fillWidth: true
                            visible: false
                            hotkeyIndex: hotkeyRow.index
                            onDone: visible = false
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}

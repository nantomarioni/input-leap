import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

Item {
    id: page

    // On a client the page shows the server's configuration as the server
    // shares it (docs/ui-redesign.md §4.7): the same controls, editable when
    // the server lets this computer change it. Options and hotkeys go back to
    // the server with the layout; its own settings one by one (setSrv).
    readonly property bool mirror: App.role === App.Client
    readonly property var cfg: App.layout
    readonly property var srv: mirror && App.mirrorAvailable ? App.sync.serverSettings : App.settings
    // the layout's options and hotkeys: read-only from a file, or when this
    // client may not change the server's configuration
    readonly property bool fromFile: !App.layoutEditable

    function setSrv(key, value) {
        App.setServerSetting(key, value)
    }

    EmptyState {
        anchors.fill: parent
        visible: page.mirror && !App.mirrorAvailable
        title: qsTr("The server's configuration lives on the server")
        description: qsTr("This computer is a client. The server's settings show here once the server shares them (Settings → Share settings on both computers), and can be changed from here once the server trusts this computer's certificate. Settings for this computer are on the Computers page.")
    }

    ScrollView {
        anchors.fill: parent
        visible: !page.mirror || App.mirrorAvailable
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
                        StatusPill {
                            text: !page.mirror ? qsTr("This computer is the server")
                                : App.serverSettingsEditable ? qsTr("held by %1 · editable here").arg(App.sync.serverName)
                                : qsTr("held by %1 · read-only").arg(App.sync.serverName)
                            tone: page.mirror && !App.serverSettingsEditable ? "info" : "success"
                            showDot: false
                        }
                        Label { text: qsTr("Changes apply right away; the server restarts by itself."); color: Theme.muted }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: page.mirror && !App.serverSettingsEditable
                text: App.layoutReadOnlyReason
                color: Theme.muted
                wrapMode: Text.WordWrap
            }

            Card {
                Layout.fillWidth: true
                padding: 14
                spacing: 6
                enabled: App.serverSettingsEditable
                SectionTitle { text: qsTr("Configuration source") }
                RadioButton {
                    text: qsTr("Generate from the layout and the options below (recommended)")
                    checked: !page.srv.useExternalConfig
                    onToggled: if (checked) page.setSrv("useExternalConfig", false)
                }
                RowLayout {
                    spacing: 8
                    RadioButton {
                        text: qsTr("Use a configuration file")
                        checked: page.srv.useExternalConfig
                        onToggled: if (checked) page.setSrv("useExternalConfig", true)
                    }
                    TextField {
                        Layout.preferredWidth: 360
                        enabled: page.srv.useExternalConfig
                        text: page.srv.configFile
                        font.family: App.monoFontFamily
                        onEditingFinished: if (text !== page.srv.configFile) page.setSrv("configFile", text)
                    }
                    Button {
                        text: qsTr("Browse…")
                        enabled: page.srv.useExternalConfig && !page.mirror   // a file on the server: typed in from a client
                        onClicked: { var f = App.pickFile(qsTr("Choose the server configuration file"), page.srv.configFile, false); if (f.length > 0) page.setSrv("configFile", f) }
                    }
                }
                Label {
                    visible: page.fromFile
                    Layout.fillWidth: true
                    text: qsTr("The options and hotkeys below, and the Layout page, show what the file says; edit the file to change them (it reloads when saved).")
                    color: Theme.muted
                    font.pixelSize: Theme.smallFontSize
                    wrapMode: Text.WordWrap
                }
                Label {
                    visible: !page.srv.useExternalConfig
                    text: qsTr("Written to %1 on every start.").arg(page.mirror ? page.srv.generatedConfigPath : App.generatedConfigPath)
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
                    enabled: App.serverSettingsEditable
                    SectionTitle { text: qsTr("Listening"); Layout.bottomMargin: 6 }
                    SettingRow {
                        label: qsTr("Address")
                        hint: qsTr("Empty listens on every interface.")
                        divider: false
                        TextField {
                            Layout.preferredWidth: 170
                            placeholderText: qsTr("all interfaces")
                            text: page.srv.networkInterface
                            font.family: App.monoFontFamily
                            onEditingFinished: if (text !== page.srv.networkInterface) page.setSrv("networkInterface", text)
                        }
                    }
                    SettingRow {
                        label: qsTr("Port")
                        NumberBox { from: 1; to: 65535; value: page.srv.port; onValueModified: page.setSrv("port", value) }
                    }
                    SettingRow {
                        label: qsTr("TLS encryption")
                        hint: qsTr("Fingerprints on the Security page.")
                        Switch { checked: page.srv.cryptoEnabled; onToggled: page.setSrv("cryptoEnabled", checked) }
                    }
                    SettingRow {
                        label: qsTr("Require client certificates")
                        Switch { checked: page.srv.requireClientCertificate; onToggled: page.setSrv("requireClientCertificate", checked) }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    padding: 14
                    spacing: 0
                    enabled: !page.fromFile
                    SectionTitle { text: qsTr("Switching screens"); Layout.bottomMargin: 6 }
                    SettingRow {
                        label: qsTr("Switch after waiting at the edge")
                        divider: false
                        NumberBox { from: 0; to: 10000; stepSize: 50; value: page.cfg.switchDelay; onValueModified: page.cfg.switchDelay = value }
                        Label { text: qsTr("ms"); color: Theme.muted }
                        Switch { checked: page.cfg.hasSwitchDelay; onToggled: page.cfg.hasSwitchDelay = checked }
                    }
                    SettingRow {
                        label: qsTr("Switch on double tap within")
                        NumberBox { from: 0; to: 10000; stepSize: 50; value: page.cfg.switchDoubleTap; onValueModified: page.cfg.switchDoubleTap = value }
                        Label { text: qsTr("ms"); color: Theme.muted }
                        Switch { checked: page.cfg.hasSwitchDoubleTap; onToggled: page.cfg.hasSwitchDoubleTap = checked }
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
                                    checked: page.cfg.switchCorners[index]
                                    onToggled: page.cfg.setSwitchCorner(index, checked)
                                }
                            }
                        }
                    }
                    SettingRow {
                        label: qsTr("Corner size")
                        NumberBox { from: 0; to: 1000; value: page.cfg.switchCornerSize; onValueModified: page.cfg.switchCornerSize = value }
                        Label { text: qsTr("px"); color: Theme.muted }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    padding: 14
                    spacing: 0
                    enabled: !page.fromFile
                    SectionTitle { text: qsTr("Behaviour"); Layout.bottomMargin: 6 }
                    SettingRow {
                        label: qsTr("Share the clipboard")
                        divider: false
                        Label { text: qsTr("up to"); color: Theme.muted }
                        NumberBox { from: 1; to: 10000; value: page.cfg.clipboardSharingSizeMb; onValueModified: page.cfg.clipboardSharingSizeMb = value }
                        Label { text: qsTr("MB"); color: Theme.muted }
                        Switch { checked: page.cfg.clipboardSharing; onToggled: page.cfg.clipboardSharing = checked }
                    }
                    SettingRow {
                        label: qsTr("Relative mouse moves")
                        hint: qsTr("For games and apps that grab the mouse.")
                        Switch { checked: page.cfg.relativeMouseMoves; onToggled: page.cfg.relativeMouseMoves = checked }
                    }
                    SettingRow {
                        label: qsTr("Synchronise screen savers")
                        Switch { checked: page.cfg.screenSaverSync; onToggled: page.cfg.screenSaverSync = checked }
                    }
                    SettingRow {
                        label: qsTr("Keep the foreground window")
                        hint: qsTr("Windows servers only.")
                        Switch { checked: page.cfg.win32KeepForeground; onToggled: page.cfg.win32KeepForeground = checked }
                    }
                    SettingRow {
                        label: qsTr("Check clients every")
                        NumberBox { from: 1000; to: 60000; stepSize: 500; value: page.cfg.heartbeat; onValueModified: page.cfg.heartbeat = value }
                        Label { text: qsTr("ms"); color: Theme.muted }
                        Switch { checked: page.cfg.hasHeartbeat; onToggled: page.cfg.hasHeartbeat = checked }
                    }
                    SettingRow {
                        label: qsTr("Drag & drop file transfer")
                        Switch { checked: page.cfg.enableDragAndDrop; onToggled: page.cfg.enableDragAndDrop = checked }
                    }
                    SettingRow {
                        label: qsTr("Ignore computers found by discovery")
                        hint: qsTr("No join requests; add computers by hand.")
                        Switch { checked: page.cfg.ignoreAutoConfigClient; onToggled: page.cfg.ignoreAutoConfigClient = checked }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    padding: 14
                    spacing: 0
                    enabled: !page.fromFile
                    SectionTitle { text: qsTr("Inactive screens"); Layout.bottomMargin: 6 }
                    SettingRow {
                        label: qsTr("Dim the screens the cursor is not on")
                        divider: false
                        Switch { checked: page.cfg.dimmingEnabled; onToggled: page.cfg.dimmingEnabled = checked }
                    }
                    SettingRow {
                        label: qsTr("Dimmed brightness")
                        Slider { Layout.preferredWidth: 140; from: 0; to: 100; stepSize: 5; value: page.cfg.dimmingPercentage; enabled: page.cfg.dimmingEnabled; onMoved: page.cfg.dimmingPercentage = value }
                        Label { text: page.cfg.dimmingPercentage + " %"; font.family: App.monoFontFamily; color: Theme.text; Layout.preferredWidth: 40 }
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

                enabled: !page.fromFile
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
                    visible: page.cfg.hotkeys.length === 0 && !newHotkey.visible
                    text: qsTr("No hotkeys yet. A hotkey runs one or more actions: switch to a screen, lock the cursor, or send a key combination.")
                    color: Theme.muted
                }

                Repeater {
                    model: page.cfg.hotkeys
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
                                        Button { text: qsTr("Remove"); flat: true; onClicked: { page.cfg.removeAction(hotkeyRow.index, index) } }
                                    }
                                }
                                Label { visible: hotkeyRow.modelData.actions.length === 0; text: qsTr("no actions yet"); color: Theme.muted }
                            }
                            Button { text: qsTr("Add action"); flat: true; onClicked: actionEditor.visible = true }
                            Button { text: qsTr("Remove hotkey"); flat: true; onClicked: { page.cfg.removeHotkey(hotkeyRow.index) } }
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

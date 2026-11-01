import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

ScrollView {
    id: page

    contentWidth: availableWidth
    clip: true

    property int selected: Math.max(0, App.computers.map(function(c) { return c.name }).indexOf(App.initialComputer))
    readonly property var computer: App.computers.length > selected ? App.computers[selected] : null
    readonly property int screenIndex: computer ? App.layout.indexOf(computer.name) : -1

    // what that computer shares: over the settings channel, or relayed by the
    // server for the other clients
    property int syncRevision: 0
    readonly property var peer: (syncRevision, computer && !computer.isSelf ? App.peerInfo(computer.name) : ({}))
    readonly property bool peerConnected: peer !== undefined && peer.connected === true
    Connections {
        target: App.sync
        function onChanged() { page.syncRevision++ }
    }
    Connections {
        target: App
        function onStateChanged() { page.syncRevision++ }
    }

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
                text: qsTr("Every computer of the fleet, this one included, the same on every computer. What the server holds and what each computer holds itself are both editable here when the computers trust each other.")
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
                        Accessible.name: modelData.name + ", " + modelData.state
                        onClicked: page.selected = index
                        Keys.onLeftPressed: page.selected = Math.max(0, page.selected - 1)
                        Keys.onRightPressed: page.selected = Math.min(App.computers.length - 1, page.selected + 1)
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
                StatusPill {
                    readonly property bool isServer: page.computer !== null && (page.computer.isSelf ? App.role === App.Server : page.peer.role === "server" || page.computer.name === App.layout.serverName)
                    text: page.computer && page.computer.isSelf ? (isServer ? qsTr("this computer · server") : qsTr("this computer · client")) : (isServer ? qsTr("server") : qsTr("client"))
                    tone: "neutral"
                    showDot: false
                }
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
                    visible: App.role === App.Server || App.mirrorAvailable
                    padding: 14
                    color: Theme.surface
                    RowLayout {
                        Layout.fillWidth: true
                        SectionTitle { text: qsTr("Held by the server") }
                        Item { Layout.fillWidth: true }
                        StatusPill {
                            text: App.layoutEditable ? qsTr("editable here") : App.role === App.Server ? qsTr("from the configuration file") : qsTr("read-only")
                            tone: App.layoutEditable ? "success" : "info"
                            showDot: false
                        }
                    }
                    ScreenSettingsPanel {
                        Layout.fillWidth: true
                        enabled: App.layoutEditable
                        visible: page.screenIndex >= 0
                        screenIndex: page.screenIndex
                        compact: true
                    }
                    Label {
                        visible: page.screenIndex < 0
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
                            text: page.computer && page.computer.isSelf ? qsTr("this computer")
                                : page.peerConnected && (page.peer.accessThere || 0) >= 2 ? qsTr("shared · editable here")
                                : page.peerConnected ? qsTr("shared · read-only") : qsTr("not shared yet")
                            tone: page.computer && page.computer.isSelf ? "info" : page.peerConnected ? "success" : "neutral"
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
                            TextField {
                                Layout.preferredWidth: 160
                                text: App.settings.screenName
                                font.family: App.monoFontFamily
                                validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9._-]{0,255}/ }
                                onEditingFinished: App.settings.screenName = text
                            }
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

                    // another computer, sharing: its own settings, editable when it
                    // trusts this computer's certificate (phase D), and its pieces
                    ColumnLayout {
                        id: remote
                        Layout.fillWidth: true
                        visible: page.computer && !page.computer.isSelf && page.peerConnected
                        spacing: 0
                        readonly property var s: page.peer.settings || ({})
                        readonly property var st: page.peer.state || ({})
                        readonly property bool writable: (page.peer.accessThere || 0) >= 2
                        readonly property string target: page.computer ? page.computer.name : ""
                        readonly property bool running: remote.st.runState === 1 || remote.st.runState === 2

                        // what the other side answered: where a change landed
                        component RemoteNote: Label {
                            property string key
                            readonly property var r: (page.syncRevision, App.remoteResult(remote.target, key))
                            Layout.fillWidth: true
                            visible: r !== undefined && r.text !== undefined && r.text.length > 0
                            text: r && r.text ? r.text : ""
                            color: !r || r.pending ? Theme.muted : r.ok ? Theme.success : Theme.danger
                            font.pixelSize: Theme.smallFontSize
                            wrapMode: Text.WordWrap
                        }

                        InfoRow { label: qsTr("Status"); value: remote.st.statusText || ""; labelWidth: 130 }
                        InfoRow { label: qsTr("Version"); value: (page.peer.version || "") + " · " + (remote.s.platform || ""); mono: true; labelWidth: 130 }
                        InfoRow { label: qsTr("Addresses"); value: (remote.s.addresses || []).join(", "); mono: true; labelWidth: 130 }
                        InfoRow {
                            label: qsTr("TLS")
                            labelWidth: 130
                            mono: true
                            value: !remote.s.cryptoEnabled ? qsTr("off")
                                 : !remote.s.fingerprint ? qsTr("on")
                                 : remote.s.fingerprint.length > 23 ? remote.s.fingerprint.substring(0, 23) + " …" : remote.s.fingerprint
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            visible: !remote.writable
                            text: page.peer.relayed
                                  ? qsTr("Read-only here: changes to %1 go through the server %2, which needs to trust this computer's certificate, and %1 needs to trust the server's.").arg(remote.target).arg(App.sync.serverName)
                                  : qsTr("Read-only here: %1 does not trust this computer's certificate, so these can only be changed on it.").arg(remote.target)
                            color: Theme.muted
                            font.pixelSize: Theme.smallFontSize
                            wrapMode: Text.WordWrap
                        }

                        SettingRow {
                            label: qsTr("Screen name")
                            Layout.topMargin: 6
                            stacked: true
                            TextField {
                                id: remoteName
                                Layout.preferredWidth: 180
                                enabled: remote.writable
                                font.family: App.monoFontFamily
                                validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9._-]{0,255}/ }
                                onEditingFinished: if (text.length > 0 && text !== remote.s.screenName) App.setRemote(remote.target, "screenName", text)
                                // follows the peer while not being edited
                                Binding { target: remoteName; property: "text"; value: remote.s.screenName || ""; when: !remoteName.activeFocus; restoreMode: Binding.RestoreNone }
                            }
                            RemoteNote { key: "screenName" }
                        }
                        SettingRow {
                            visible: remote.s.role === "client"
                            label: qsTr("Server to connect to")
                            hint: qsTr("Empty uses what its discovery finds.")
                            stacked: true
                            TextField {
                                id: remoteServer
                                Layout.preferredWidth: 220
                                enabled: remote.writable
                                placeholderText: qsTr("found by discovery")
                                font.family: App.monoFontFamily
                                onEditingFinished: if (text !== (remote.s.serverHostname || "")) App.setRemote(remote.target, "serverHostname", text)
                                Binding { target: remoteServer; property: "text"; value: remote.s.serverHostname || ""; when: !remoteServer.activeFocus; restoreMode: Binding.RestoreNone }
                            }
                            RemoteNote { key: "serverHostname" }
                        }
                        SettingRow {
                            visible: remote.s.role === "server"
                            label: qsTr("Listening on")
                            Label { text: (remote.s.networkInterface || qsTr("all interfaces")) + " · " + qsTr("port %1").arg(remote.s.port || ""); font.family: App.monoFontFamily; color: Theme.muted }
                        }
                        SettingRow {
                            label: qsTr("Find computers with Bonjour")
                            stacked: true
                            Switch { enabled: remote.writable; checked: remote.s.discovery === true; onToggled: App.setRemote(remote.target, "discovery", checked) }
                            RemoteNote { key: "discovery" }
                        }
                        SettingRow {
                            visible: remote.s.role === "client"
                            label: qsTr("Scroll step")
                            stacked: true
                            NumberBox { enabled: remote.writable; from: 1; to: 10000; value: remote.s.scrollStep || 120; onValueModified: App.setRemote(remote.target, "scrollStep", value) }
                            RemoteNote { key: "scrollStep" }
                        }
                        SettingRow {
                            label: qsTr("Overlay hints")
                            stacked: true
                            Switch { enabled: remote.writable; checked: remote.s.overlayHints === true; onToggled: App.setRemote(remote.target, "overlayHints", checked) }
                            RemoteNote { key: "overlayHints" }
                        }
                        SettingRow {
                            label: qsTr("Log level")
                            stacked: true
                            ComboBox {
                                Layout.preferredWidth: 140
                                enabled: remote.writable
                                model: [qsTr("Error"), qsTr("Warning"), qsTr("Note"), qsTr("Info"), qsTr("Debug"), qsTr("Debug 1"), qsTr("Debug 2")]
                                currentIndex: remote.s.logLevel || 0
                                onActivated: (index) => App.setRemote(remote.target, "logLevel", index)
                            }
                            RemoteNote { key: "logLevel" }
                        }
                        SettingRow {
                            label: qsTr("Launch at login")
                            stacked: true
                            Switch { enabled: remote.writable; checked: remote.s.launchAtLogin === true; onToggled: App.setRemote(remote.target, "launchAtLogin", checked) }
                            RemoteNote { key: "launchAtLogin" }
                        }
                        SettingRow {
                            label: qsTr("Start its core at launch")
                            stacked: true
                            Switch { enabled: remote.writable; checked: remote.s.startCoreOnLaunch === true; onToggled: App.setRemote(remote.target, "startCoreOnLaunch", checked) }
                            RemoteNote { key: "startCoreOnLaunch" }
                        }

                        Label { Layout.topMargin: 10; text: qsTr("Its pieces"); font.pixelSize: Theme.smallFontSize; font.weight: Font.DemiBold; color: Theme.muted }
                        Repeater {
                            model: page.peer.processes || []
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                spacing: 8
                                Rectangle { width: 8; height: 8; radius: 4; color: !modelData.relevant ? Theme.borderSoft : modelData.ok ? Theme.success : Theme.offline }
                                Label { text: modelData.name; color: Theme.text; Layout.preferredWidth: 150; elide: Text.ElideRight }
                                Label { Layout.fillWidth: true; text: modelData.detail; color: Theme.muted; elide: Text.ElideRight }
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            Layout.topMargin: 10
                            spacing: 8
                            visible: remote.writable
                            Button { text: qsTr("Restart its core"); enabled: remote.st.runState === 2; onClicked: App.remoteAction(remote.target, "restart") }
                            Button { text: remote.running ? qsTr("Stop it") : qsTr("Start it"); onClicked: App.remoteAction(remote.target, remote.running ? "stop" : "start") }
                            Button { text: qsTr("Restart its discovery"); flat: true; onClicked: App.remoteAction(remote.target, "restartDiscovery") }
                        }
                        RemoteNote { key: "action"; Layout.topMargin: 4 }
                    }

                    // another computer, not sharing yet
                    Label {
                        visible: page.computer && !page.computer.isSelf && !page.peerConnected
                        Layout.fillWidth: true
                        text: qsTr("Its own settings (name, server address, discovery, overlay hints, log level, its pieces) show here once it shares them with the server: Share settings on, and the server trusting its certificate. Clients see each other through the server.")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }
}

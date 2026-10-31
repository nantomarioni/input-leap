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
        spacing: 14

        ColumnLayout {
            spacing: 2
            Label { text: qsTr("Processes"); font.pixelSize: Theme.titleFontSize; font.weight: Font.DemiBold; color: Theme.text }
            Label {
                Layout.fillWidth: true
                text: qsTr("Input Leap is a few cooperating pieces on this computer. Each one shows its state here and can be restarted on its own.")
                color: Theme.muted
                wrapMode: Text.WordWrap
            }
        }

        // The chain, left to right.
        Card {
            Layout.fillWidth: true
            padding: 10
            RowLayout {
                spacing: 0
                StatusPill { text: qsTr("App · this window"); tone: "info"; showDot: false }
                Label { text: App.core.serviceMode ? "  → ipc 24801 →  " : "  → launches →  "; font.family: App.monoFontFamily; color: Theme.muted }
                StatusPill { visible: App.core.serviceMode; text: qsTr("Service · input-leapd"); tone: "info"; showDot: false }
                Label { visible: App.core.serviceMode; text: "  → launches →  "; font.family: App.monoFontFamily; color: Theme.muted }
                StatusPill { text: App.role === App.Server ? qsTr("Core · input-leaps") : qsTr("Core · input-leapc"); tone: "info"; showDot: false }
                Label { text: "  → tcp " + App.settings.port + " →  "; font.family: App.monoFontFamily; color: Theme.muted }
                StatusPill {
                    text: App.role === App.Server ? qsTr("%1 clients").arg(App.connectedClients.length) : (App.serverAddress.length > 0 ? App.serverAddress : qsTr("the server"))
                    tone: App.connected ? "success" : "neutral"
                    showDot: false
                }
                Item { Layout.fillWidth: true }
                StatusPill { text: qsTr("Discovery · Bonjour"); tone: App.discovery.active ? "success" : "neutral"; showDot: false }
                Label { text: "  ·  "; color: Theme.muted }
                StatusPill { text: qsTr("Settings channel · tls %1").arg(App.sync.port); tone: App.sync.listening ? "success" : "neutral"; showDot: false }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 14
            rowSpacing: 14

            // ---- app ----------------------------------------------------------
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4; color: Theme.success }
                    Label { text: qsTr("App"); font.weight: Font.DemiBold; font.pixelSize: 14; color: Theme.text }
                    Label { Layout.fillWidth: true; text: qsTr("this window and the tray icon"); color: Theme.muted; elide: Text.ElideRight }
                }
                InfoRow { label: qsTr("Version"); value: App.version; mono: true; labelWidth: 110 }
                InfoRow { label: qsTr("Updates"); value: App.updateStatus.length > 0 ? App.updateStatus : qsTr("not checked yet"); labelWidth: 110 }
                InfoRow { label: qsTr("Settings"); value: App.dataFolder; mono: true; labelWidth: 110 }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Button { text: qsTr("Check for updates"); onClicked: App.checkForUpdates() }
                    Button { visible: App.updateInstallable; text: qsTr("Install and relaunch"); highlighted: true; onClicked: App.installUpdate() }
                    Button { text: qsTr("Open data folder"); onClicked: App.openDataFolder() }
                }
            }

            // ---- core ---------------------------------------------------------
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4; color: App.core.state === CoreProcess.Running ? Theme.success : App.core.state === CoreProcess.Failed ? Theme.danger : Theme.offline }
                    Label { text: App.role === App.Server ? qsTr("Core · input-leaps") : qsTr("Core · input-leapc"); font.weight: Font.DemiBold; font.pixelSize: 14; color: Theme.text }
                    Label { Layout.fillWidth: true; text: App.role === App.Server ? qsTr("the server process that captures input") : qsTr("the client process that injects input"); color: Theme.muted; elide: Text.ElideRight }
                }
                InfoRow {
                    label: qsTr("State")
                    labelWidth: 110
                    value: App.core.state === CoreProcess.Running
                           ? (App.core.pid > 0 ? qsTr("Running · PID %1 · since %2").arg(App.core.pid).arg(Qt.formatTime(App.core.startedAt, "HH:mm:ss"))
                                               : qsTr("Running through the service"))
                           : App.core.state === CoreProcess.Starting ? qsTr("Starting")
                           : App.core.state === CoreProcess.Stopping ? qsTr("Stopping")
                           : App.core.state === CoreProcess.Failed ? qsTr("Failed · %1").arg(App.core.lastError)
                           : qsTr("Not running")
                }
                InfoRow { label: qsTr("Log level"); value: App.settings.logLevel >= 0 ? ["ERROR", "WARNING", "NOTE", "INFO", "DEBUG", "DEBUG1", "DEBUG2"][App.settings.logLevel] : ""; labelWidth: 110; mono: true }
                InfoRow { label: qsTr("Started by"); value: App.core.serviceMode ? qsTr("the service, elevated as configured") : qsTr("this app, as a child process"); labelWidth: 110 }
                InfoRow { label: qsTr("Command"); value: App.core.commandLine.length > 0 ? App.core.commandLine : qsTr("—"); mono: true; labelWidth: 110 }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Button { text: qsTr("Restart"); enabled: App.runState === App.Running; onClicked: App.restart() }
                    Button { text: App.canSwitchRole ? qsTr("Start") : qsTr("Stop"); onClicked: App.canSwitchRole ? App.start() : App.stop() }
                    Button { text: qsTr("Copy command"); enabled: App.core.commandLine.length > 0; onClicked: App.appendLog(qsTr("command copied: %1").arg(App.core.commandLine)) }
                }
            }

            // ---- service ------------------------------------------------------
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                opacity: App.core.serviceMode ? 1 : 0.75
                RowLayout {
                    spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4; color: App.core.serviceMode ? (App.core.serviceConnected ? Theme.success : Theme.danger) : Theme.offline }
                    Label { text: qsTr("Service · input-leapd"); font.weight: Font.DemiBold; font.pixelSize: 14; color: Theme.text }
                    StatusPill { text: qsTr("Windows"); tone: "neutral"; showDot: false }
                }
                InfoRow {
                    label: qsTr("State")
                    labelWidth: 110
                    value: App.core.serviceMode
                           ? (App.core.serviceConnected ? qsTr("Connected over IPC on 127.0.0.1:24801") : qsTr("Not reachable over IPC"))
                           : qsTr("Not used on %1: the app launches the core itself").arg(App.platformName)
                }
                InfoRow {
                    visible: App.core.serviceMode
                    label: qsTr("Elevation")
                    labelWidth: 110
                    value: [qsTr("as needed · restarts the core when a UAC desktop appears"), qsTr("always"), qsTr("never")][App.settings.elevateMode]
                }
                RowLayout {
                    visible: App.core.serviceMode
                    spacing: 8
                    Button { text: qsTr("Reconnect IPC"); onClicked: App.core.reconnectService() }
                }
            }

            // ---- discovery ----------------------------------------------------
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4; color: App.discovery.active && App.discovery.error.length === 0 ? Theme.success : Theme.offline }
                    Label { text: qsTr("Discovery · Bonjour"); font.weight: Font.DemiBold; font.pixelSize: 14; color: Theme.text }
                    Label { Layout.fillWidth: true; text: qsTr("finds the other computers on the LAN"); color: Theme.muted; elide: Text.ElideRight }
                }
                InfoRow {
                    label: qsTr("State")
                    labelWidth: 110
                    value: !App.discovery.available ? App.discovery.unavailableReason
                         : !App.settings.discovery ? qsTr("Turned off in the settings")
                         : App.discovery.error.length > 0 ? App.discovery.error
                         : App.discovery.active ? qsTr("Running") : qsTr("Not started")
                }
                InfoRow { visible: App.discovery.active; label: qsTr("Advertising"); value: App.discovery.advertising; mono: true; labelWidth: 110 }
                InfoRow {
                    visible: App.discovery.active
                    label: App.role === App.Server ? qsTr("Clients found") : qsTr("Servers found")
                    labelWidth: 110
                    value: (App.role === App.Server ? App.discovery.clients : App.discovery.servers).join(", ") || qsTr("none yet")
                    mono: true
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Button { text: qsTr("Restart discovery"); enabled: App.discovery.available && App.settings.discovery; onClicked: App.discovery.restart() }
                    Button {
                        text: App.settings.discovery ? qsTr("Turn off") : qsTr("Turn on")
                        enabled: App.discovery.available
                        onClicked: { App.settings.discovery = !App.settings.discovery; if (!App.settings.discovery) App.discovery.stop(); else App.discovery.restart() }
                    }
                }
            }

            // ---- the settings channel ----------------------------------------
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4; color: App.sync.listening ? Theme.success : Theme.offline }
                    Label { text: qsTr("Settings channel"); font.weight: Font.DemiBold; font.pixelSize: 14; color: Theme.text }
                    Label { Layout.fillWidth: true; text: qsTr("shares settings with the trusted computers, read-only"); color: Theme.muted; elide: Text.ElideRight }
                }
                InfoRow {
                    label: qsTr("State")
                    labelWidth: 110
                    value: !App.settings.syncEnabled ? qsTr("Turned off in the settings")
                         : App.sync.listening ? qsTr("Listening on port %1 over TLS (%2)").arg(App.sync.port).arg(App.sync.backend)
                         : (App.sync.error.length > 0 ? App.sync.error : qsTr("Not started"))
                }
                InfoRow {
                    visible: App.sync.listening
                    label: App.role === App.Server ? qsTr("Clients sharing") : qsTr("Server")
                    labelWidth: 110
                    mono: true
                    value: App.role === App.Server
                           ? (App.sync.peers.map(function(p) { return p.name }).join(", ") || qsTr("none yet"))
                           : (App.sync.serverConnected ? App.sync.serverName : qsTr("not reached yet"))
                }
                InfoRow { label: qsTr("Trust"); labelWidth: 110; value: qsTr("the certificates the core already trusts; names from the layout when client certificates are not required") }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Button { text: qsTr("Restart channel"); enabled: App.settings.syncEnabled; onClicked: App.restartSync() }
                    Button {
                        text: App.settings.syncEnabled ? qsTr("Turn off") : qsTr("Turn on")
                        onClicked: App.settings.syncEnabled = !App.settings.syncEnabled
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }
}

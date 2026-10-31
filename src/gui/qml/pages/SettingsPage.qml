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
            Label { text: qsTr("Settings"); font.pixelSize: Theme.titleFontSize; font.weight: Font.DemiBold; color: Theme.text }
            Label { text: qsTr("The app itself. Server and client behaviour live on their own pages."); color: Theme.muted }
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
                SectionTitle { text: qsTr("General"); Layout.bottomMargin: 6 }
                SettingRow {
                    label: qsTr("This computer's screen name")
                    hint: qsTr("The name the other computers know it by. Changing it while running takes effect on restart.")
                    divider: false
                    TextField {
                        Layout.preferredWidth: 150
                        text: App.settings.screenName
                        font.family: App.monoFontFamily
                        onEditingFinished: App.settings.screenName = text
                    }
                }
                SettingRow {
                    label: qsTr("Appearance")
                    ComboBox {
                        Layout.preferredWidth: 160
                        model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                        currentIndex: App.settings.colorScheme === "light" ? 1 : App.settings.colorScheme === "dark" ? 2 : 0
                        onActivated: (index) => App.settings.colorScheme = ["system", "light", "dark"][index]
                    }
                }
                SettingRow {
                    label: qsTr("Language")
                    hint: qsTr("English only in this version.")
                    Label { text: qsTr("English"); color: Theme.muted }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                spacing: 0
                SectionTitle { text: qsTr("Startup & window"); Layout.bottomMargin: 6 }
                SettingRow {
                    label: qsTr("Launch Input Leap at login")
                    divider: false
                    Switch { checked: App.settings.launchAtLogin; onToggled: App.setLaunchAtLogin(checked) }
                }
                SettingRow {
                    label: App.role === App.Server ? qsTr("Start the server automatically") : qsTr("Connect automatically")
                    hint: qsTr("When the app launches, after the first successful start.")
                    Switch { checked: App.settings.startCoreOnLaunch; onToggled: App.settings.startCoreOnLaunch = checked }
                }
                SettingRow {
                    label: qsTr("Open hidden in the tray")
                    Switch { checked: App.settings.openHidden; onToggled: App.settings.openHidden = checked }
                }
                SettingRow {
                    label: qsTr("Closing the window keeps running in the tray")
                    hint: qsTr("Quit from the tray menu.")
                    Label { text: qsTr("always"); color: Theme.muted }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                spacing: 0
                SectionTitle { text: qsTr("Notifications"); Layout.bottomMargin: 6 }
                SettingRow {
                    label: qsTr("On-screen overlay")
                    hint: qsTr("Connected, dimmed, cursor locked.")
                    divider: false
                    Switch { checked: App.settings.overlayHints; onToggled: App.settings.overlayHints = checked }
                }
                SettingRow {
                    label: qsTr("System notifications")
                    hint: qsTr("Join and trust requests, new clients, while the window is hidden.")
                    Switch { checked: App.settings.systemNotifications; onToggled: App.settings.systemNotifications = checked }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                spacing: 0
                SectionTitle { text: qsTr("Logging"); Layout.bottomMargin: 6 }
                SettingRow {
                    label: qsTr("Level")
                    hint: qsTr("Applies to the core on its next start.")
                    divider: false
                    ComboBox {
                        Layout.preferredWidth: 160
                        model: [qsTr("Error"), qsTr("Warning"), qsTr("Note"), qsTr("Info"), qsTr("Debug"), qsTr("Debug 1"), qsTr("Debug 2")]
                        currentIndex: App.settings.logLevel
                        onActivated: (index) => App.settings.logLevel = index
                    }
                }
                SettingRow {
                    label: qsTr("Also write the core's log to a file")
                    Switch { checked: App.settings.logToFile; onToggled: App.settings.logToFile = checked }
                }
                SettingRow {
                    label: qsTr("File")
                    divider: false
                    stacked: true
                    TextField {
                        Layout.fillWidth: true
                        enabled: App.settings.logToFile
                        text: App.settings.logFilename
                        font.family: App.monoFontFamily
                        onEditingFinished: App.settings.logFilename = text
                    }
                    Button { text: qsTr("Open folder"); onClicked: App.openLogsFolder() }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                spacing: 0
                SectionTitle { text: qsTr("Updates"); Layout.bottomMargin: 6 }
                SettingRow {
                    label: qsTr("Check daily, install with one click")
                    hint: qsTr("Follows the rolling latest-build release of the fork.")
                    divider: false
                    Switch { checked: App.settings.autoCheckUpdates; onToggled: App.settings.autoCheckUpdates = checked }
                }
                SettingRow {
                    label: qsTr("Installed %1").arg(App.version)
                    hint: App.updateStatus
                    Button { text: qsTr("Check now"); onClicked: App.checkForUpdates() }
                    Button { visible: App.updateInstallable; text: qsTr("Install"); highlighted: true; onClicked: App.installUpdate() }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                padding: 14
                spacing: 0
                visible: App.isWindows
                SectionTitle { text: qsTr("Windows service"); Layout.bottomMargin: 6 }
                SettingRow {
                    label: qsTr("Run the core elevated")
                    hint: qsTr("\"As needed\" restarts the core on the secure desktop so UAC prompts and the lock screen stay reachable.")
                    divider: false
                    ComboBox {
                        Layout.preferredWidth: 160
                        model: [qsTr("As needed"), qsTr("Always"), qsTr("Never")]
                        currentIndex: App.settings.elevateMode
                        onActivated: (index) => App.settings.elevateMode = index
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

ApplicationWindow {
    id: root

    width: 1040
    height: 760
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Input Leap"
    color: Theme.bg

    property int currentPage: 0

    // Closing keeps Input Leap running in the tray; Quit lives in the tray menu.
    onClosing: (close) => {
        close.accepted = false
        root.hide()
    }

    header: AppHeader {}

    footer: AppStatusBar {
        logOpen: App.settings.logDrawerOpen
        onToggleLog: App.settings.logDrawerOpen = !App.settings.logDrawerOpen
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavRail {
            Layout.fillHeight: true
            currentIndex: root.currentPage
            onActivated: (index) => root.currentPage = index
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentPage

                OverviewPage {}
                LayoutPage {}
                ServerPage {}
                ComputersPage {}
                SecurityPage {}
                ProcessesPage {}
                SettingsPage {}
            }

            LogDrawer {
                Layout.fillWidth: true
                open: App.settings.logDrawerOpen
            }
        }
    }
}

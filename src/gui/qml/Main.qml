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

    header: AppHeader {}

    footer: AppStatusBar {
        logOpen: logDrawer.open
        onToggleLog: logDrawer.open = !logDrawer.open
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
                id: logDrawer
                Layout.fillWidth: true
            }
        }
    }
}

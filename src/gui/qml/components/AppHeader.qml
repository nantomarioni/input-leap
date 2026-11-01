import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

Rectangle {
    id: header

    readonly property bool stopped: App.runState === App.Stopped || App.runState === App.Failed

    implicitHeight: 56
    color: Theme.surface

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 16

        RowLayout {
            Layout.minimumWidth: 150
            spacing: 10

            Image {
                source: "../icons/logo.svg"
                sourceSize: Qt.size(24, 24)
            }
            Label {
                text: "Input Leap"
                font.pixelSize: 15
                font.weight: Font.Bold
                color: Theme.text
            }
        }

        RoleSwitch {}

        Item { Layout.fillWidth: true }

        StatusPill {
            text: App.statusText
            tone: App.runState === App.Failed ? "danger"
                : App.runState === App.Running && App.connected ? "success"
                : App.runState === App.Running ? "info"
                : header.stopped ? "neutral" : "warning"
        }

        Button {
            text: qsTr("Restart")
            enabled: App.runState === App.Running
            onClicked: App.restart()
            ToolTip.visible: hovered
            ToolTip.delay: 600
            ToolTip.text: qsTr("Restart this computer's core")
        }

        // every computer's core: the server asks the clients that let it
        Button {
            text: qsTr("Restart all")
            visible: App.computers.length > 1 && (App.role === App.Server || App.serverSettingsEditable)
            onClicked: App.restartFleet()
            ToolTip.visible: hovered
            ToolTip.delay: 600
            ToolTip.text: qsTr("Restart the core on every computer of the fleet")
        }

        Button {
            text: header.stopped ? qsTr("Start") : qsTr("Stop")
            highlighted: true
            enabled: App.runState !== App.Stopping
            onClicked: header.stopped ? App.start() : App.stop()
        }
    }
}

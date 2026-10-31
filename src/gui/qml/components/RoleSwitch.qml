import QtQuick
import QtQuick.Controls
import InputLeap.Ui

// Server | Client. Enabled only while the core is stopped (plan §4.2).
Rectangle {
    id: control

    implicitWidth: row.implicitWidth + 4
    implicitHeight: row.implicitHeight + 4
    radius: height / 2
    color: Theme.surface
    border.color: Theme.border
    border.width: 1
    opacity: App.canSwitchRole ? 1 : 0.7

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("This computer's role")

    HoverHandler { id: hover }

    ToolTip.visible: !App.canSwitchRole && hover.hovered
    ToolTip.delay: 400
    ToolTip.text: qsTr("Stop Input Leap to change the role")

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 0

        RoleOption {
            roleValue: App.Server
            text: qsTr("Server")
        }
        RoleOption {
            roleValue: App.Client
            text: qsTr("Client")
        }
    }
}

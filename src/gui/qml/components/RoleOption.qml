import QtQuick
import QtQuick.Controls
import InputLeap.Ui

AbstractButton {
    id: option

    property int roleValue: App.Server
    readonly property bool active: App.role === roleValue

    implicitWidth: label.implicitWidth + 28
    implicitHeight: 28
    enabled: App.canSwitchRole || active
    hoverEnabled: true

    Accessible.role: Accessible.RadioButton
    Accessible.checked: active

    onClicked: App.role = roleValue

    background: Rectangle {
        radius: height / 2
        color: option.active ? Theme.accent
             : option.hovered && option.enabled ? Theme.hover
             : "transparent"
    }

    contentItem: Label {
        id: label
        text: option.text
        color: option.active ? Theme.onAccent : Theme.text
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

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
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.RadioButton
    Accessible.name: text
    Accessible.checked: active

    onClicked: App.role = roleValue
    Keys.onReturnPressed: option.clicked()
    Keys.onEnterPressed: option.clicked()
    Keys.onLeftPressed: if (App.canSwitchRole) App.role = App.Server
    Keys.onRightPressed: if (App.canSwitchRole) App.role = App.Client

    background: Rectangle {
        radius: height / 2
        color: option.active ? Theme.accent
             : option.hovered && option.enabled ? Theme.hover
             : "transparent"
        border.color: option.visualFocus ? Theme.accent : "transparent"
        border.width: option.visualFocus ? 2 : 0
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

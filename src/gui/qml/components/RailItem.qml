import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import InputLeap.Ui

AbstractButton {
    id: item

    property string iconName
    property bool active: false

    implicitHeight: 36
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.PageTab
    Accessible.name: text
    Accessible.selected: active

    Keys.onReturnPressed: item.clicked()
    Keys.onEnterPressed: item.clicked()

    background: Rectangle {
        radius: Theme.radiusSmall
        color: item.active ? Theme.surface
             : item.hovered ? Theme.hover
             : "transparent"
        border.color: item.visualFocus ? Theme.accent : "transparent"
        border.width: item.visualFocus ? 2 : 0

        Rectangle {
            visible: item.active
            width: 3
            height: parent.height
            radius: 1.5
            color: Theme.accent
        }
    }

    contentItem: RowLayout {
        spacing: 10

        IconImage {
            Layout.leftMargin: 10
            source: "../icons/" + item.iconName + ".svg"
            sourceSize: Qt.size(16, 16)
            color: item.active ? Theme.accent : Theme.muted
        }
        Label {
            Layout.fillWidth: true
            text: item.text
            color: item.active ? Theme.accent : Theme.text
            font.weight: Font.Medium
            elide: Text.ElideRight
        }
    }
}

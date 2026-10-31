import QtQuick
import QtQuick.Layouts
import InputLeap.Ui

// A surface with border and radius; children stack vertically.
Rectangle {
    id: card

    default property alias content: column.data
    property int padding: Theme.pad
    property int spacing: Theme.gap

    implicitWidth: column.implicitWidth + 2 * padding
    implicitHeight: column.implicitHeight + 2 * padding
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    ColumnLayout {
        id: column
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: card.padding
        spacing: card.spacing
    }
}

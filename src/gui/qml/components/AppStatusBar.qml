import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

Rectangle {
    id: bar

    property bool logOpen: false
    signal toggleLog()

    implicitHeight: 40
    color: Theme.surface

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 12
        spacing: 12

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: App.runState === App.Running ? Theme.success
                 : App.runState === App.Failed ? Theme.danger
                 : Theme.offline
        }
        Label {
            text: App.statusText
            color: Theme.text
        }
        Label {
            Layout.fillWidth: true
            text: App.lastEvent.length > 0 ? "· " + App.lastEvent : ""
            color: Theme.muted
            elide: Text.ElideRight
        }
        Label {
            text: qsTr("Closing the window keeps Input Leap running in the tray")
            color: Theme.muted
            font.pixelSize: Theme.smallFontSize
        }
        Button {
            text: bar.logOpen ? qsTr("Hide log") : qsTr("Show log")
            flat: true
            onClicked: bar.toggleLog()
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

Rectangle {
    id: rail

    property int currentIndex: 0
    signal activated(int index)

    readonly property var pages: [
        { label: qsTr("Overview"),  icon: "overview" },
        { label: qsTr("Layout"),    icon: "layout" },
        { label: qsTr("Server"),    icon: "server" },
        { label: qsTr("Computers"), icon: "computers" },
        { label: qsTr("Security"),  icon: "security" },
        { label: qsTr("Processes"), icon: "processes" },
        { label: qsTr("Settings"),  icon: "settings" }
    ]

    implicitWidth: 196
    color: Theme.rail

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        anchors.topMargin: 14
        spacing: 2

        Repeater {
            model: rail.pages

            delegate: RailItem {
                required property int index
                required property var modelData

                Layout.fillWidth: true
                text: modelData.label
                iconName: modelData.icon
                active: index === rail.currentIndex
                onClicked: rail.activated(index)
            }
        }

        Item { Layout.fillHeight: true }

        ColumnLayout {
            Layout.leftMargin: 10
            Layout.bottomMargin: 4
            spacing: 6

            StatusPill {
                visible: App.updateAvailable
                text: qsTr("Update available")
                tone: "info"
                showDot: false
            }
            Label {
                text: App.version
                color: Theme.muted
                font.family: App.monoFontFamily
                font.pixelSize: 11
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
        }
    }
}

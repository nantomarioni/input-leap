import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// Bottom drawer with the live log, available on every page.
Rectangle {
    id: drawer

    property bool open: false

    implicitHeight: open ? 220 : 0
    clip: true
    color: Theme.surface

    Behavior on implicitHeight {
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 12
            Layout.topMargin: 6
            Layout.bottomMargin: 4
            spacing: 8

            Label {
                text: qsTr("Log")
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                text: qsTr("· live, 10 000 lines kept")
                color: Theme.muted
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Clear")
                flat: true
                onClicked: App.clearLog()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.bottomMargin: 8
            clip: true
            model: App.logLines
            delegate: Label {
                required property string modelData
                width: ListView.view.width
                text: modelData
                font.family: App.monoFontFamily
                font.pixelSize: 12
                color: Theme.text
            }
            onCountChanged: Qt.callLater(positionViewAtEnd)
            ScrollBar.vertical: ScrollBar {}
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// Bottom drawer with the live log, available on every page.
Rectangle {
    id: drawer

    property bool open: false

    implicitHeight: open ? 240 : 0
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
                text: qsTr("· %1 lines").arg(App.log.count)
                color: Theme.muted
            }
            Item { width: 6 }

            Repeater {
                model: [
                    { label: qsTr("All"), source: "" },
                    { label: qsTr("App"), source: "app" },
                    { label: qsTr("Core"), source: "core" },
                    { label: qsTr("Service"), source: "service" }
                ]
                delegate: Button {
                    required property var modelData
                    text: modelData.label
                    flat: App.log.source !== modelData.source
                    highlighted: App.log.source === modelData.source
                    onClicked: App.log.source = modelData.source
                }
            }

            ComboBox {
                id: levelBox
                Layout.preferredWidth: 150
                model: [qsTr("Errors only"), qsTr("Warnings"), qsTr("Notes"), qsTr("Info"), qsTr("Debug"), qsTr("Debug 1"), qsTr("Debug 2")]
                currentIndex: App.log.minLevel
                onActivated: (index) => App.log.minLevel = index
            }
            TextField {
                Layout.preferredWidth: 180
                placeholderText: qsTr("Search…")
                text: App.log.search
                onTextEdited: App.log.search = text
            }

            Item { Layout.fillWidth: true }

            Button { text: qsTr("Copy"); flat: true; onClicked: App.copyLogToClipboard() }
            Button { text: qsTr("Clear"); flat: true; onClicked: App.logModel.clear() }
            Button { text: qsTr("Hide"); flat: true; onClicked: App.settings.logDrawerOpen = false }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.bottomMargin: 8
            clip: true
            model: App.log
            delegate: RowLayout {
                required property string time
                required property string source
                required property string level
                required property string text
                width: ListView.view.width
                spacing: 8
                Label { text: time; font.family: App.monoFontFamily; font.pixelSize: 12; color: Theme.muted }
                Label {
                    text: source
                    font.family: App.monoFontFamily
                    font.pixelSize: 12
                    Layout.preferredWidth: 52
                    color: source === "core" ? Theme.accent : source === "service" ? Theme.warning : Theme.success
                }
                Label {
                    text: level
                    font.family: App.monoFontFamily
                    font.pixelSize: 12
                    Layout.preferredWidth: 56
                    color: level === "ERROR" || level === "FATAL" ? Theme.danger : level === "WARNING" ? Theme.warning : Theme.muted
                }
                Label {
                    Layout.fillWidth: true
                    text: text
                    font.family: App.monoFontFamily
                    font.pixelSize: 12
                    color: Theme.text
                    elide: Text.ElideRight
                }
            }
            onCountChanged: Qt.callLater(positionViewAtEnd)
            ScrollBar.vertical: ScrollBar {}
        }
    }
}

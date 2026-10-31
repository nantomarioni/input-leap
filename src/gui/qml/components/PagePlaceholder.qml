import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// Stand-in for a page that a later milestone fills in.
Item {
    id: page

    property string title
    property string description
    property string milestone

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 20
        spacing: 6

        Label {
            text: page.title
            font.pixelSize: Theme.titleFontSize
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Label {
            Layout.fillWidth: true
            text: page.description
            color: Theme.muted
            wrapMode: Text.WordWrap
        }
        StatusPill {
            visible: page.milestone.length > 0
            text: qsTr("Planned for %1").arg(page.milestone)
            tone: "neutral"
            showDot: false
        }
        Item { Layout.fillHeight: true }
    }
}

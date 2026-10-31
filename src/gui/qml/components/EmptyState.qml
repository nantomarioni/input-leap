import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// A page that has nothing to show yet, with a reason.
Item {
    property string title
    property string description

    Card {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 520)
        padding: 24
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            Label { Layout.fillWidth: true; text: title; font.pixelSize: Theme.titleFontSize; font.weight: Font.DemiBold; color: Theme.text; wrapMode: Text.WordWrap }
            Label { Layout.fillWidth: true; text: description; color: Theme.muted; wrapMode: Text.WordWrap }
        }
    }
}

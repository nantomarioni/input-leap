import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// A read-only fact: "Screen name   desk-win".
RowLayout {
    property string label
    property string value
    property bool mono: false
    property int labelWidth: 120

    Layout.fillWidth: true
    spacing: 12

    Label {
        Layout.preferredWidth: labelWidth
        text: label
        color: Theme.muted
    }
    Label {
        Layout.fillWidth: true
        text: value
        color: Theme.text
        font.family: mono ? App.monoFontFamily : Qt.application.font.family
        wrapMode: Text.WrapAnywhere
    }
}

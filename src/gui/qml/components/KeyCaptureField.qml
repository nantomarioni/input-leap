import QtQuick
import QtQuick.Controls
import InputLeap.Ui

// A box that records keys: click it, press the combination.
Rectangle {
    id: field

    property alias capture: keys
    property string placeholder: qsTr("Click, then press the keys")

    implicitWidth: 220
    implicitHeight: 32
    radius: Theme.radiusSmall
    color: keys.recording ? Theme.accentSoft : Theme.surface
    border.color: keys.recording ? Theme.accent : Theme.border
    border.width: 1

    KeyCapture {
        id: keys
        anchors.fill: parent
    }

    Label {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        text: keys.recording ? (keys.text.length > 0 ? keys.text + " …" : qsTr("Recording… press the keys"))
            : keys.valid ? keys.text
            : field.placeholder
        font.family: keys.valid || keys.recording ? App.monoFontFamily : Qt.application.font.family
        color: keys.valid || keys.recording ? Theme.text : Theme.muted
    }
}

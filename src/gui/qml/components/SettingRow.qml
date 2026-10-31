import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// One setting: a label (and hint) on the left, the controls on the right.
ColumnLayout {
    id: row

    property string label
    property string hint
    property bool divider: true
    property bool stacked: false     // controls under the label, full width
    default property alias controls: slot.data

    Layout.fillWidth: true
    spacing: 0

    Rectangle {
        Layout.fillWidth: true
        visible: row.divider
        height: 1
        color: Theme.borderSoft
    }

    GridLayout {
        Layout.fillWidth: true
        Layout.topMargin: 8
        Layout.bottomMargin: 8
        columns: row.stacked ? 1 : 2
        columnSpacing: 12
        rowSpacing: 6

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Label { Layout.fillWidth: true; text: row.label; color: Theme.text; wrapMode: Text.WordWrap }
            Label {
                Layout.fillWidth: true
                visible: row.hint.length > 0
                text: row.hint
                color: Theme.muted
                font.pixelSize: Theme.smallFontSize
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            id: slot
            Layout.fillWidth: row.stacked
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            spacing: 8
        }
    }
}

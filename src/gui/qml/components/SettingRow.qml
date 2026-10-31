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

    // A control without a name of its own (a Switch, a ComboBox, a field) is
    // announced with the row's label; buttons and labels keep their text.
    Component.onCompleted: {
        for (let i = 0; i < slot.children.length; ++i) {
            const c = slot.children[i]
            if (c instanceof Label || c instanceof Text || c.Accessible === undefined)
                continue
            if (c.checkable === false && c.text)
                continue
            if (!c.Accessible.name)
                c.Accessible.name = row.label
        }
    }

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

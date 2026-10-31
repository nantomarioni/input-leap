import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// Inline editor: a new hotkey with its first action, or one more action for
// an existing hotkey (hotkeyIndex >= 0).
Card {
    id: editor

    property int hotkeyIndex: -1
    signal done()

    readonly property var typeIds: ["keystroke", "keyDown", "keyUp", "switchToScreen", "toggleScreen", "switchInDirection", "lockCursorToScreen"]
    readonly property bool keyAction: typeBox.currentIndex <= 2
    property var selectedScreens: []

    color: Theme.surfaceAlt
    padding: 14

    function submit() {
        var index = editor.hotkeyIndex
        if (index < 0) {
            index = App.addHotkey(hotkeyField.capture)
            if (index < 0) return
        }
        var action = {
            type: typeIds[typeBox.currentIndex],
            screens: editor.keyAction ? editor.selectedScreens : [],
            switchScreen: screenBox.currentText,
            direction: directionBox.currentIndex,
            lockMode: lockBox.currentIndex,
            onRelease: releaseBox.checked
        }
        if (App.addHotkeyAction(index, action, actionField.capture)) {
            editor.done()
        } else if (editor.hotkeyIndex < 0) {
            App.layout.removeHotkey(index)
        }
    }

    RowLayout {
        visible: editor.hotkeyIndex < 0
        spacing: 12
        Label { text: qsTr("Hotkey"); color: Theme.text; Layout.preferredWidth: 120 }
        KeyCaptureField { id: hotkeyField; placeholder: qsTr("Click, then press the hotkey") }
        Label { Layout.fillWidth: true; text: qsTr("A key combination or a mouse button."); color: Theme.muted; elide: Text.ElideRight }
    }

    RowLayout {
        spacing: 12
        Label { text: qsTr("Action"); color: Theme.text; Layout.preferredWidth: 120 }
        ComboBox {
            id: typeBox
            Layout.preferredWidth: 240
            model: [qsTr("Press and release a key"), qsTr("Press a key"), qsTr("Release a key"),
                    qsTr("Switch to a screen"), qsTr("Toggle screen"), qsTr("Switch in a direction"), qsTr("Lock the cursor to the screen")]
        }
    }

    RowLayout {
        visible: editor.keyAction
        spacing: 12
        Label { text: qsTr("Keys to send"); color: Theme.text; Layout.preferredWidth: 120 }
        KeyCaptureField { id: actionField }
    }

    ColumnLayout {
        visible: editor.keyAction && App.layout.screenNames.length > 0
        spacing: 4
        Label { text: qsTr("Only on these screens (none = all)"); color: Theme.muted; font.pixelSize: Theme.smallFontSize }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: App.layout.screenNames
                delegate: CheckBox {
                    required property string modelData
                    text: modelData
                    onToggled: {
                        var list = editor.selectedScreens.slice()
                        var at = list.indexOf(modelData)
                        if (checked && at < 0) list.push(modelData)
                        if (!checked && at >= 0) list.splice(at, 1)
                        editor.selectedScreens = list
                    }
                }
            }
        }
    }

    RowLayout {
        visible: typeBox.currentIndex === 3
        spacing: 12
        Label { text: qsTr("Screen"); color: Theme.text; Layout.preferredWidth: 120 }
        ComboBox { id: screenBox; Layout.preferredWidth: 240; model: App.layout.screenNames }
    }

    RowLayout {
        visible: typeBox.currentIndex === 5
        spacing: 12
        Label { text: qsTr("Direction"); color: Theme.text; Layout.preferredWidth: 120 }
        ComboBox { id: directionBox; Layout.preferredWidth: 160; model: [qsTr("left"), qsTr("right"), qsTr("up"), qsTr("down")] }
    }

    RowLayout {
        visible: typeBox.currentIndex === 6
        spacing: 12
        Label { text: qsTr("Lock"); color: Theme.text; Layout.preferredWidth: 120 }
        ComboBox { id: lockBox; Layout.preferredWidth: 160; model: [qsTr("toggle"), qsTr("on"), qsTr("off")] }
    }

    CheckBox { id: releaseBox; text: qsTr("Perform when the hotkey is released rather than pressed") }

    RowLayout {
        Layout.alignment: Qt.AlignRight
        spacing: 8
        Button { text: qsTr("Cancel"); onClicked: editor.done() }
        Button { text: editor.hotkeyIndex < 0 ? qsTr("Add hotkey") : qsTr("Add action"); highlighted: true; onClicked: editor.submit() }
    }
}

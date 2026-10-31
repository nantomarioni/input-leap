import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// The options the server holds for one computer in the layout: name,
// aliases, modifier remaps, dead corners and the keyboard fixes.
ColumnLayout {
    id: panel

    property int cell: -1
    property bool compact: false

    // bump to re-read after the model changed
    property int revision: 0
    readonly property var s: (revision, App.layout.screenSettings(cell))
    readonly property bool valid: cell >= 0 && s.name !== undefined && s.name.length > 0

    Connections {
        target: App.layout
        function onLayoutChanged() { panel.revision++ }
    }

    function update(patch) {
        App.layout.setScreenSettings(panel.cell, patch)
    }

    spacing: 10

    ColumnLayout {
        spacing: 4
        Label { text: qsTr("Name"); font.pixelSize: Theme.smallFontSize; font.weight: Font.DemiBold; color: Theme.muted }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: nameField
                width: 200
                text: panel.valid ? panel.s.name : ""
                font.family: App.monoFontFamily
                validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9._-]{0,255}/ }
                onEditingFinished: if (panel.valid && text !== panel.s.name) App.layout.renameScreen(panel.cell, text)
            }
            StatusPill { visible: panel.valid && panel.s.isServer; text: qsTr("this computer"); tone: "info"; showDot: false }
        }
    }

    ColumnLayout {
        spacing: 4
        Label { text: qsTr("Aliases"); font.pixelSize: Theme.smallFontSize; font.weight: Font.DemiBold; color: Theme.muted }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: panel.valid ? panel.s.aliases : []
                delegate: Button {
                    required property string modelData
                    text: modelData + "  ×"
                    font.family: App.monoFontFamily
                    flat: true
                    onClicked: {
                        var list = panel.s.aliases.filter(function(a) { return a !== modelData })
                        panel.update({ aliases: list })
                    }
                }
            }
        }
        TextField {
            Layout.preferredWidth: 220
            placeholderText: qsTr("Add an alias and press Enter")
            validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9._-]{0,255}/ }
            onAccepted: {
                if (text.length === 0) return
                var list = panel.s.aliases.slice()
                list.push(text)
                panel.update({ aliases: list })
                text = ""
            }
        }
    }

    ColumnLayout {
        spacing: 6
        Label { text: qsTr("Dead corners · no switching there"); font.pixelSize: Theme.smallFontSize; font.weight: Font.DemiBold; color: Theme.muted }
        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 2
            Repeater {
                model: [qsTr("Top-left"), qsTr("Top-right"), qsTr("Bottom-left"), qsTr("Bottom-right")]
                delegate: CheckBox {
                    required property int index
                    required property string modelData
                    text: modelData
                    checked: panel.valid ? panel.s.corners[index] : false
                    onToggled: {
                        var corners = panel.s.corners.slice()
                        corners[index] = checked
                        panel.update({ corners: corners })
                    }
                }
            }
        }
        RowLayout {
            spacing: 8
            Label { text: qsTr("Corner size"); color: Theme.text }
            NumberBox {
                from: 0; to: 1000
                value: panel.valid ? panel.s.cornerSize : 0
                onValueModified: panel.update({ cornerSize: value })
            }
            Label { text: qsTr("px"); color: Theme.muted }
        }
    }

    ColumnLayout {
        spacing: 4
        Label { text: qsTr("Modifier keys on this computer"); font.pixelSize: Theme.smallFontSize; font.weight: Font.DemiBold; color: Theme.muted }
        GridLayout {
            columns: panel.compact ? 1 : 3
            columnSpacing: 10
            rowSpacing: 4
            Repeater {
                model: [qsTr("Shift"), qsTr("Ctrl"), qsTr("Alt"), qsTr("Meta"), qsTr("Super")]
                delegate: RowLayout {
                    required property int index
                    required property string modelData
                    spacing: 6
                    Label { text: modelData; color: Theme.text; Layout.preferredWidth: 44 }
                    ComboBox {
                        Layout.preferredWidth: 110
                        model: [qsTr("Shift"), qsTr("Ctrl"), qsTr("Alt"), qsTr("Meta"), qsTr("Super"), qsTr("None")]
                        currentIndex: panel.valid ? panel.s.modifiers[index] : index
                        onActivated: (i) => {
                            var mods = panel.s.modifiers.slice()
                            mods[index] = i
                            panel.update({ modifiers: mods })
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        spacing: 2
        Label { text: qsTr("Fixes"); font.pixelSize: Theme.smallFontSize; font.weight: Font.DemiBold; color: Theme.muted }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: [qsTr("Caps Lock"), qsTr("Num Lock"), qsTr("Scroll Lock"), qsTr("XTest for Xinerama"), qsTr("Preserve focus")]
                delegate: CheckBox {
                    required property int index
                    required property string modelData
                    text: modelData
                    checked: panel.valid ? panel.s.fixes[index] : false
                    onToggled: {
                        var fixes = panel.s.fixes.slice()
                        fixes[index] = checked
                        panel.update({ fixes: fixes })
                    }
                }
            }
        }
    }
}

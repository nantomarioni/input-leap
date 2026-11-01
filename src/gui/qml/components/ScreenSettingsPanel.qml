import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// The options the server holds for one computer in the layout: name,
// displays, aliases, modifier remaps, dead corners and the keyboard fixes.
ColumnLayout {
    id: panel

    property int screenIndex: -1
    property bool compact: false

    // bump to re-read after the model changed
    property int revision: 0
    readonly property var s: (revision, App.layout.screenSettings(screenIndex))
    readonly property bool valid: screenIndex >= 0 && s.name !== undefined && s.name.length > 0

    Connections {
        target: App.layout
        function onLayoutChanged() { panel.revision++ }
    }

    function update(patch) {
        return App.layout.setScreenSettings(panel.screenIndex, patch)
    }

    // one field of one display; the model keeps the desktop's origin at (0, 0)
    function setDisplay(index, patch) {
        var list = panel.s.displays.slice()
        var d = Object.assign({}, list[index], patch)
        list[index] = d
        if (!panel.update({ displays: list }))
            panel.revision++   // refused (it would run into a neighbour): show the old values again
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
                Accessible.name: qsTr("Screen name")
                text: panel.valid ? panel.s.name : ""
                font.family: App.monoFontFamily
                validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9._-]{0,255}/ }
                onEditingFinished: if (panel.valid && text !== panel.s.name) App.layout.renameScreen(panel.screenIndex, text)
            }
            StatusPill { visible: panel.valid && panel.s.isServer; text: qsTr("this computer"); tone: "info"; showDot: false }
        }
    }

    // ---- displays: size and place of each, in the computer's own pixels ----
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Displays"); font.pixelSize: Theme.smallFontSize; font.weight: Font.DemiBold; color: Theme.muted }
            Item { Layout.fillWidth: true }
            Label {
                text: panel.valid ? qsTr("desktop %1 × %2").arg(panel.s.width).arg(panel.s.height) : ""
                font.family: App.monoFontFamily
                font.pixelSize: Theme.smallFontSize
                color: Theme.muted
            }
        }
        Label {
            Layout.fillWidth: true
            visible: panel.valid && panel.s.displaysReported && !panel.s.reportDiffers
            text: qsTr("As %1 reports them; they follow its monitors.").arg(panel.s.name)
            color: Theme.muted
            font.pixelSize: Theme.smallFontSize
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            visible: panel.valid && panel.s.reportDiffers
            spacing: 8
            Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: panel.valid && !panel.s.displaysTyped
                      ? qsTr("%1 reports %n display(s) that would run into a neighbour; move it apart and try again.", "", panel.s.reportedDisplays.length).arg(panel.s.name)
                      : qsTr("Typed by hand; %1 reports %n display(s).", "", panel.valid ? panel.s.reportedDisplays.length : 0).arg(panel.valid ? panel.s.name : "")
                color: Theme.warning
                font.pixelSize: Theme.smallFontSize
                wrapMode: Text.WordWrap
            }
            Button {
                text: panel.valid && !panel.s.displaysTyped ? qsTr("Try again") : qsTr("Use them")
                onClicked: if (!App.layout.useReportedDisplays(panel.screenIndex)) panel.revision++
            }
        }
        Repeater {
            model: panel.valid ? panel.s.displays : []
            delegate: ColumnLayout {
                id: displayRow
                required property int index
                required property var modelData
                spacing: 2
                RowLayout {
                    spacing: 6
                    Label { text: displayRow.index + 1; color: Theme.text; font.weight: Font.DemiBold; Layout.preferredWidth: 14 }
                    TextField {
                        Layout.preferredWidth: 70
                        text: displayRow.modelData.width
                        validator: IntValidator { bottom: 1; top: 100000 }
                        horizontalAlignment: TextInput.AlignRight
                        font.family: App.monoFontFamily
                        Accessible.name: qsTr("Display %1 width").arg(displayRow.index + 1)
                        onEditingFinished: {
                            const n = parseInt(text)
                            if (!isNaN(n) && n !== displayRow.modelData.width)
                                panel.setDisplay(displayRow.index, { width: n })
                            else
                                text = displayRow.modelData.width
                        }
                    }
                    Label { text: "×"; color: Theme.muted }
                    TextField {
                        Layout.preferredWidth: 70
                        text: displayRow.modelData.height
                        validator: IntValidator { bottom: 1; top: 100000 }
                        horizontalAlignment: TextInput.AlignRight
                        font.family: App.monoFontFamily
                        Accessible.name: qsTr("Display %1 height").arg(displayRow.index + 1)
                        onEditingFinished: {
                            const n = parseInt(text)
                            if (!isNaN(n) && n !== displayRow.modelData.height)
                                panel.setDisplay(displayRow.index, { height: n })
                            else
                                text = displayRow.modelData.height
                        }
                    }
                    Label {
                        visible: displayRow.modelData.mmWidth > 0
                        text: qsTr("%1 × %2 mm").arg(displayRow.modelData.mmWidth).arg(displayRow.modelData.mmHeight)
                        color: Theme.muted
                        font.pixelSize: Theme.smallFontSize
                    }
                    ToolButton {
                        text: "×"
                        visible: panel.valid && panel.s.displays.length > 1
                        Accessible.name: qsTr("Remove display %1").arg(displayRow.index + 1)
                        onClicked: App.layout.removeDisplay(panel.screenIndex, displayRow.index)
                    }
                }
                RowLayout {
                    spacing: 6
                    Label { text: qsTr("at"); color: Theme.muted; Layout.leftMargin: 20 }
                    TextField {
                        Layout.preferredWidth: 70
                        text: displayRow.modelData.x
                        validator: IntValidator { bottom: -100000; top: 100000 }
                        horizontalAlignment: TextInput.AlignRight
                        font.family: App.monoFontFamily
                        Accessible.name: qsTr("Display %1 horizontal offset").arg(displayRow.index + 1)
                        onEditingFinished: {
                            const n = parseInt(text)
                            if (!isNaN(n) && n !== displayRow.modelData.x)
                                panel.setDisplay(displayRow.index, { x: n })
                            else
                                text = displayRow.modelData.x
                        }
                    }
                    Label { text: ","; color: Theme.muted }
                    TextField {
                        Layout.preferredWidth: 70
                        text: displayRow.modelData.y
                        validator: IntValidator { bottom: -100000; top: 100000 }
                        horizontalAlignment: TextInput.AlignRight
                        font.family: App.monoFontFamily
                        Accessible.name: qsTr("Display %1 vertical offset").arg(displayRow.index + 1)
                        onEditingFinished: {
                            const n = parseInt(text)
                            if (!isNaN(n) && n !== displayRow.modelData.y)
                                panel.setDisplay(displayRow.index, { y: n })
                            else
                                text = displayRow.modelData.y
                        }
                    }
                }
            }
        }
        Button { text: qsTr("Add display"); flat: true; onClicked: App.layout.addDisplay(panel.screenIndex) }
        Label {
            Layout.fillWidth: true
            text: qsTr("Pixels as that computer's core sees them; offsets place a display beside the first. Typing here stops the reports from replacing them.")
            color: Theme.muted
            font.pixelSize: Theme.smallFontSize
            wrapMode: Text.WordWrap
        }
    }

    // ---- how big its pixels are next to the others' ----
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 4
        Label { text: qsTr("Size on the desk"); font.pixelSize: Theme.smallFontSize; font.weight: Font.DemiBold; color: Theme.muted }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            NumberBox {
                from: 20; to: 400
                Accessible.name: qsTr("Size of this computer's pixels, in percent of a 96-dpi pixel")
                value: panel.valid ? Math.round(panel.s.scale) : 100
                onValueModified: if (!App.layout.setScale(panel.screenIndex, value)) panel.revision++
            }
            Label { text: "%"; color: Theme.muted }
            Item { Layout.fillWidth: true }
            Button {
                visible: panel.valid && panel.s.scaleTyped && panel.s.measuredScale > 0
                text: qsTr("Use measured (%1 %)").arg(panel.valid ? Math.round(panel.s.measuredScale) : 0)
                flat: true
                onClicked: if (!App.layout.useMeasuredScale(panel.screenIndex)) panel.revision++
            }
        }
        Label {
            Layout.fillWidth: true
            text: !panel.valid ? ""
                : panel.s.scaleTyped ? qsTr("Typed: the monitors' real sizes no longer change it.")
                : panel.s.measuredScale > 0 ? qsTr("From the monitors' real sizes, so every computer is drawn as big as it is and the cursor crosses where the screens really meet. 100 % is a 96-dpi pixel.")
                : qsTr("The monitors don't say how big they are: set it so this computer is drawn at its real size next to the others. 100 % is a 96-dpi pixel.")
            color: Theme.muted
            font.pixelSize: Theme.smallFontSize
            wrapMode: Text.WordWrap
        }
    }

    // ---- links across a gap ----
    ColumnLayout {
        id: gapSection
        Layout.fillWidth: true
        spacing: 4
        visible: panel.valid && App.layout.screenCount >= 2
        readonly property var links: (panel.revision, App.layout.gapLinksOf(panel.screenIndex))
        readonly property var sideNames: [qsTr("Left edge"), qsTr("Right edge"), qsTr("Top edge"), qsTr("Bottom edge")]
        Label { text: qsTr("Links across a gap"); font.pixelSize: Theme.smallFontSize; font.weight: Font.DemiBold; color: Theme.muted }
        Label {
            Layout.fillWidth: true
            text: qsTr("For screens that don't touch: the edge leads straight to the other computer, and back, where no neighbour touches it.")
            color: Theme.muted
            font.pixelSize: Theme.smallFontSize
            wrapMode: Text.WordWrap
        }
        Repeater {
            model: gapSection.links
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: qsTr("%1 → %2").arg(gapSection.sideNames[modelData.side]).arg(modelData.name)
                    color: Theme.text
                    elide: Text.ElideRight
                }
                Button {
                    text: "×"
                    flat: true
                    Accessible.name: qsTr("Remove the link to %1").arg(modelData.name)
                    onClicked: { App.layout.removeGapLink(panel.screenIndex, modelData.side, modelData.other); panel.revision++ }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ComboBox {
                id: gapSide
                Layout.fillWidth: true
                model: gapSection.sideNames
                currentIndex: 1
                Accessible.name: qsTr("Edge of this computer")
            }
            Label { text: qsTr("leads to"); color: Theme.muted }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ComboBox {
                id: gapTarget
                Layout.fillWidth: true
                model: (panel.revision, App.layout.screenNames.filter(function(n) { return panel.valid && n !== panel.s.name }))
                Accessible.name: qsTr("Computer it leads to")
            }
            Button {
                text: qsTr("Link")
                enabled: gapTarget.currentIndex >= 0
                onClicked: if (App.layout.addGapLink(panel.screenIndex, gapSide.currentIndex, App.layout.indexOf(gapTarget.currentText))) panel.revision++
            }
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
            Accessible.name: qsTr("New alias")
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
                Accessible.name: qsTr("Corner size in pixels")
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
                        Accessible.name: qsTr("%1 acts as").arg(modelData)
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

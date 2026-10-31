import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// The desktops drawn to scale: every computer is the bounding box of its
// displays, placed where the server's layout says; desktops whose edges
// touch are linked. Interactive on the Layout page (drag, arrow keys),
// a plain picture on the Overview.
Item {
    id: canvas

    property bool interactive: false
    property bool showDetails: true      // state/position under the name, display outlines
    property int selectedIndex: -1
    property int padding: 16
    property real maxScale: 0.14         // a 1920 px desktop is at most ~270 px wide
    signal selected(int index)
    signal rejected()                    // a drop that would overlap another computer

    readonly property rect bounds: App.layout.bounds
    readonly property real pxScale: {
        const b = bounds
        if (b.width <= 0 || b.height <= 0)
            return maxScale
        const sx = (width - 2 * padding) / b.width
        const sy = (height - 2 * padding) / b.height
        return Math.max(0.004, Math.min(maxScale, sx, sy))
    }
    readonly property real originX: (width - bounds.width * pxScale) / 2 - bounds.x * pxScale
    readonly property real originY: (height - bounds.height * pxScale) / 2 - bounds.y * pxScale

    function focusTile(index) {
        const item = tiles.itemAt(index)
        if (item)
            item.focusTile()
    }

    // Snap to a nearby edge, then move; refuse (and say so) when the place is taken.
    function dropAt(index, x, y, snapThreshold) {
        const snapped = App.layout.snapPosition(index, x, y, snapThreshold)
        if (App.layout.moveTo(index, snapped.x, snapped.y))
            return true
        if (!snapped.snapped || !App.layout.moveTo(index, x, y)) {
            rejected()
            return false
        }
        return true
    }

    Repeater {
        id: tiles
        model: App.layout

        delegate: Item {
            id: slot
            required property int index
            required property string name
            required property int desktopX
            required property int desktopY
            required property int desktopWidth
            required property int desktopHeight
            required property var displays
            required property bool isServer
            required property bool linked
            required property string state
            required property string position

            x: canvas.originX + desktopX * canvas.pxScale
            y: canvas.originY + desktopY * canvas.pxScale
            width: Math.max(8, desktopWidth * canvas.pxScale)
            height: Math.max(8, desktopHeight * canvas.pxScale)
            z: mouse.drag.active ? 10 : 0

            readonly property bool current: canvas.selectedIndex === index
            readonly property int nudge: Math.max(1, Math.round(desktopWidth / 4))

            function focusTile() { tile.forceActiveFocus(Qt.TabFocusReason) }

            function moveBy(dx, dy) {
                // one step, snapping to whatever edge is within reach
                canvas.dropAt(index, desktopX + dx, desktopY + dy, Math.round(Math.max(desktopWidth, desktopHeight) * 0.3))
                canvas.selected(index)
                Qt.callLater(function() { canvas.focusTile(index) })
            }

            Rectangle {
                id: tile
                width: slot.width
                height: slot.height
                radius: Math.min(6, width / 8)
                color: slot.isServer ? Theme.accentSoft : Theme.surface
                border.color: slot.current || tile.activeFocus ? Theme.accent
                            : slot.isServer ? Theme.accent
                            : slot.linked ? Theme.border : Theme.warning
                border.width: slot.current || tile.activeFocus ? 2 : 1
                opacity: mouse.drag.active ? 0.85 : 1

                activeFocusOnTab: canvas.interactive
                Accessible.role: Accessible.Button
                Accessible.name: slot.isServer ? qsTr("%1, this computer").arg(slot.name) : slot.name + ", " + slot.position
                Accessible.focusable: canvas.interactive
                onActiveFocusChanged: if (activeFocus) canvas.selected(slot.index)
                Keys.onLeftPressed: slot.moveBy(-slot.nudge, 0)
                Keys.onRightPressed: slot.moveBy(slot.nudge, 0)
                Keys.onUpPressed: slot.moveBy(0, -slot.nudge)
                Keys.onDownPressed: slot.moveBy(0, slot.nudge)
                Keys.onPressed: (event) => {
                    if ((event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) && canvas.interactive && !slot.isServer) {
                        App.layout.removeScreen(slot.index)
                        canvas.selected(-1)
                        event.accepted = true
                    }
                }

                // the displays, when there is more than one
                Repeater {
                    model: canvas.showDetails && slot.displays.length > 1 ? slot.displays : []
                    delegate: Rectangle {
                        required property int index
                        required property var modelData
                        x: modelData.x * canvas.pxScale
                        y: modelData.y * canvas.pxScale
                        width: modelData.width * canvas.pxScale
                        height: modelData.height * canvas.pxScale
                        radius: 3
                        color: "transparent"
                        border.color: Theme.borderSoft
                        border.width: 1
                        Label {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 3
                            text: index + 1
                            font.pixelSize: 9
                            color: Theme.muted
                        }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    width: parent.width - 10
                    spacing: 1
                    Label {
                        width: parent.width
                        text: slot.name
                        font.weight: Font.DemiBold
                        font.pixelSize: slot.width < 90 ? 11 : canvas.showDetails ? 13 : 11
                        color: slot.isServer ? Theme.accent : Theme.text
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        width: parent.width
                        visible: canvas.showDetails && slot.height >= 44
                        text: slot.isServer ? qsTr("this computer")
                            : slot.state === "connected" ? qsTr("connected")
                            : slot.state === "offline" ? qsTr("offline")
                            : slot.position
                        font.pixelSize: 10
                        color: slot.state === "connected" ? Theme.success : slot.linked ? Theme.muted : Theme.warning
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    enabled: canvas.interactive
                    drag.target: canvas.interactive ? tile : null
                    drag.threshold: 4
                    onPressed: { canvas.selected(slot.index); tile.forceActiveFocus(Qt.MouseFocusReason) }
                    onReleased: {
                        const dx = Math.round(tile.x / canvas.pxScale)
                        const dy = Math.round(tile.y / canvas.pxScale)
                        tile.x = 0
                        tile.y = 0
                        if (dx !== 0 || dy !== 0)
                            canvas.dropAt(slot.index, slot.desktopX + dx, slot.desktopY + dy, Math.round(16 / canvas.pxScale))
                    }
                }
            }
        }
    }
}

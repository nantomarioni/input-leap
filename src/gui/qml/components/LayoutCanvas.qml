import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import InputLeap.Ui

// The desktops drawn at their real sizes: every computer is its displays,
// placed where the server's layout says. Where a display of each of two
// touching desktops meets the other, the cursor crosses (the accent lines);
// the rest of every edge is a wall. Interactive on the Layout page (drag,
// arrow keys), a plain picture on the Overview.
Item {
    id: canvas

    property var model: App.layout       // on a client, its copy of the server's
    property bool interactive: false
    property bool showDetails: true      // state/position under the name, display outlines
    property int selectedIndex: -1
    property int padding: 16
    property real maxScale: 0.14         // a 1920 px desktop is at most ~270 px wide
    signal selected(int index)
    signal rejected()                    // a drop that would overlap another computer

    readonly property rect bounds: model.bounds
    property int revision: 0
    readonly property var crossings: (revision, model.crossings())
    readonly property var gapCrossings: (revision, model.gapCrossings())

    Connections {
        target: canvas.model
        function onLayoutChanged() { canvas.revision++ }
        function onModelReset() { canvas.revision++ }
    }
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
        const snapped = canvas.model.snapPosition(index, x, y, snapThreshold)
        if (canvas.model.moveTo(index, snapped.x, snapped.y))
            return true
        if (!snapped.snapped || !canvas.model.moveTo(index, x, y)) {
            rejected()
            return false
        }
        return true
    }

    Repeater {
        id: tiles
        model: canvas.model

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
            readonly property bool isSelf: slot.name.toLowerCase() === App.screenName.toLowerCase()
            readonly property bool online: slot.isServer || slot.isSelf || slot.state === "connected"
            readonly property int nudge: Math.max(1, Math.round(desktopWidth / 4))

            function focusTile() { tile.forceActiveFocus(Qt.TabFocusReason) }

            function moveBy(dx, dy) {
                // one step, snapping to whatever edge is within reach
                canvas.dropAt(index, desktopX + dx, desktopY + dy, Math.round(Math.max(desktopWidth, desktopHeight) * 0.3))
                canvas.selected(index)
                Qt.callLater(function() { canvas.focusTile(index) })
            }

            // the display the name goes on: the biggest
            readonly property int mainDisplay: {
                let best = 0
                for (let i = 1; i < displays.length; ++i)
                    if (displays[i].width * displays[i].height > displays[best].width * displays[best].height)
                        best = i
                return best
            }
            readonly property var main: displays.length > 0 ? displays[mainDisplay]
                                                            : { x: 0, y: 0, width: desktopWidth, height: desktopHeight }

            Rectangle {
                id: tile
                width: slot.width
                height: slot.height
                radius: 4
                // the desktop's outline, only where it matters: picked, with gaps between displays
                color: "transparent"
                border.color: Theme.accent
                border.width: (slot.current || tile.activeFocus) && slot.displays.length > 1 ? 1 : 0
                // a computer that is not connected stays where it sits, grayed out
                opacity: mouse.drag.active ? 0.85 : slot.online ? 1 : 0.45

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
                        canvas.model.removeScreen(slot.index)
                        canvas.selected(-1)
                        event.accepted = true
                    }
                }

                // every display as it is; the space between them is no screen
                Repeater {
                    model: slot.displays
                    delegate: Rectangle {
                        required property int index
                        required property var modelData
                        x: modelData.x * canvas.pxScale
                        y: modelData.y * canvas.pxScale
                        width: Math.max(4, modelData.width * canvas.pxScale)
                        height: Math.max(4, modelData.height * canvas.pxScale)
                        radius: Math.min(5, width / 8)
                        color: slot.isServer ? Theme.accentSoft : Theme.surface
                        border.color: slot.current || tile.activeFocus ? Theme.accent
                                    : slot.isServer ? Theme.accent
                                    : slot.linked ? Theme.border : Theme.warning
                        border.width: slot.current || tile.activeFocus ? 2 : 1
                        Label {
                            visible: canvas.showDetails && slot.displays.length > 1
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
                    x: slot.main.x * canvas.pxScale + 5
                    y: (slot.main.y + slot.main.height / 2) * canvas.pxScale - height / 2
                    width: slot.main.width * canvas.pxScale - 10
                    spacing: 1
                    Label {
                        width: parent.width
                        text: slot.name
                        font.weight: Font.DemiBold
                        font.pixelSize: slot.main.width * canvas.pxScale < 90 ? 11 : canvas.showDetails ? 13 : 11
                        color: slot.isServer ? Theme.accent : Theme.text
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        width: parent.width
                        visible: canvas.showDetails && slot.main.height * canvas.pxScale >= 44
                        text: slot.isServer ? (canvas.model.isMirror ? qsTr("server") : qsTr("this computer"))
                            : canvas.model.isMirror && slot.isSelf ? qsTr("this computer")
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

    // where the cursor crosses from one computer to the next
    Repeater {
        model: canvas.crossings
        delegate: Rectangle {
            required property var modelData
            readonly property bool upright: modelData.x1 === modelData.x2
            x: canvas.originX + modelData.x1 * canvas.pxScale - (upright ? 2 : 0)
            y: canvas.originY + modelData.y1 * canvas.pxScale - (upright ? 0 : 2)
            width: upright ? 4 : Math.max(2, (modelData.x2 - modelData.x1) * canvas.pxScale)
            height: upright ? Math.max(2, (modelData.y2 - modelData.y1) * canvas.pxScale) : 4
            radius: 2
            color: Theme.success
            z: 20
        }
    }

    // links made by hand across a gap: edge to edge
    Repeater {
        model: canvas.gapCrossings
        delegate: Rectangle {
            required property var modelData
            readonly property real dx: (modelData.x2 - modelData.x1) * canvas.pxScale
            readonly property real dy: (modelData.y2 - modelData.y1) * canvas.pxScale
            x: canvas.originX + modelData.x1 * canvas.pxScale
            y: canvas.originY + modelData.y1 * canvas.pxScale - height / 2
            width: Math.max(4, Math.sqrt(dx * dx + dy * dy))
            height: 3
            radius: 1.5
            transformOrigin: Item.Left
            rotation: Math.atan2(dy, dx) * 180 / Math.PI
            color: Theme.success
            opacity: 0.7
            z: 20
        }
    }
}

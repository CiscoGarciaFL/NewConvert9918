pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// A 1:1 TMS9918A screen grid. Each editor slot is represented by one
// independently positioned 8x8 tile; duplicate patterns remain linked for
// selection/highlighting while retaining separate coordinates.
Item {
    id: root
    required property real zoomScale

    readonly property var slots: editorProject.characterEditorSlots
    readonly property int slotCount: slots.length
    readonly property int renderedTileCount: tileRepeater.count
    readonly property var activeSlot:
        slotCount > 0 ? slots[editorProject.activeCharacterEditor] : null
    readonly property int relatedTileCount: {
        if (!activeSlot || !activeSlot.loaded)
            return 0
        let count = 0
        for (let index = 0; index < slots.length; ++index) {
            const slot = slots[index]
            if (slot.loaded && slot.setIndex === activeSlot.setIndex
                    && slot.patternIndex === activeSlot.patternIndex) {
                ++count
            }
        }
        return count
    }

    function paletteColor(index) {
        const colors = editorProject.characterPaletteColors
        if (index < 0 || index >= colors.length)
            return "#000000"
        if (index === 0)
            return "#303842"
        return colors[index]
    }

    Flickable {
        id: tilingFlick
        objectName: "characterTilingFlickable"
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: Math.max(width, screenCanvas.width + 8)
        contentHeight: Math.max(height, screenCanvas.height + 8)
        ScrollBar.horizontal: ScrollBar {}
        ScrollBar.vertical: ScrollBar {}

        Rectangle {
            id: screenCanvas
            objectName: "characterTilingScreenGrid"
            x: 4
            y: 4
            width: 256 * root.zoomScale
            height: 192 * root.zoomScale
            color: "#111820"
            border.width: 1
            border.color: "#73808c"
            clip: true

            Repeater {
                model: 33
                Rectangle {
                    required property int index
                    x: Math.min(screenCanvas.width - 1,
                                index * 8 * root.zoomScale)
                    width: 1
                    height: screenCanvas.height
                    color: index % 4 === 0 ? "#394854" : "#28343e"
                }
            }
            Repeater {
                model: 25
                Rectangle {
                    required property int index
                    y: Math.min(screenCanvas.height - 1,
                                index * 8 * root.zoomScale)
                    width: screenCanvas.width
                    height: 1
                    color: index % 4 === 0 ? "#394854" : "#28343e"
                }
            }

            // The emphasized corner is screen coordinate 0,0 (Home).
            Rectangle {
                width: 12
                height: 2
                color: palette.highlight
                z: 2000
            }
            Rectangle {
                width: 2
                height: 12
                color: palette.highlight
                z: 2000
            }

            Repeater {
                id: tileRepeater
                objectName: "characterTileRepeater"
                model: root.slotCount

                delegate: Item {
                    id: tile
                    objectName: "characterTile"
                    required property int index
                    readonly property var slotData: root.slots[index]
                    readonly property var patternRows: {
                        const revision = editorProject.characterRevision
                        if (!slotData.loaded)
                            return []
                        return editorProject.characterPatternRows(
                            slotData.setIndex, slotData.patternIndex)
                    }
                    readonly property bool active:
                        index === editorProject.activeCharacterEditor
                    readonly property bool relatedPattern:
                        !active && slotData.loaded && root.activeSlot
                        && root.activeSlot.loaded
                        && slotData.setIndex === root.activeSlot.setIndex
                        && slotData.patternIndex === root.activeSlot.patternIndex
                    x: slotData.tileX * root.zoomScale
                    y: slotData.tileY * root.zoomScale
                    width: 8 * root.zoomScale
                    height: 8 * root.zoomScale
                    z: active ? 1000 : relatedPattern ? 500 : index + 10

                    Repeater {
                        model: 64
                        Rectangle {
                            required property int index
                            readonly property int patternRow: Math.floor(index / 8)
                            readonly property int patternColumn: index % 8
                            readonly property bool foregroundPixel:
                                tile.slotData.loaded && tile.patternRows.length === 8
                                && (tile.patternRows[patternRow].pattern
                                    & (0x80 >> patternColumn)) !== 0
                            x: patternColumn * root.zoomScale
                            y: patternRow * root.zoomScale
                            width: root.zoomScale
                            height: root.zoomScale
                            color: !tile.slotData.loaded
                                   || tile.patternRows.length !== 8 ? "#20272e"
                                   : root.paletteColor(foregroundPixel
                                                       ? tile.patternRows[patternRow].foreground
                                                       : tile.patternRows[patternRow].background)
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.width: 1
                        border.color: tile.active ? "#ffffff"
                                      : tile.relatedPattern ? "#62c5ff"
                                      : tile.slotData.loaded ? "#68737f"
                                      : "#d6a64b"
                    }

                    Item {
                        anchors.fill: parent
                        visible: !tile.slotData.loaded
                        Rectangle {
                            anchors.centerIn: parent
                            width: tile.width
                            height: 1
                            rotation: 45
                            color: "#d6a64b"
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: tile.width
                            height: 1
                            rotation: -45
                            color: "#d6a64b"
                        }
                    }

                    Rectangle {
                        id: hoverLabel
                        visible: tileHover.hovered
                        z: 3000
                        width: hoverText.implicitWidth + 8
                        height: 18
                        x: Math.max(-tile.x,
                                    Math.min((tile.width - width) / 2,
                                             screenCanvas.width - tile.x - width))
                        y: tile.y >= height + 2 ? -height - 2 : tile.height + 2
                        color: "#202a33"
                        border.width: 1
                        border.color: tile.active ? "#ffffff" : "#73808c"
                        radius: 2

                        Label {
                            id: hoverText
                            anchors.centerIn: parent
                            text: tile.slotData.loaded
                                  ? qsTr("Pattern %1")
                                        .arg(Number(tile.slotData.patternIndex)
                                             .toString(16).toUpperCase()
                                             .padStart(2, "0"))
                                  : qsTr("Empty")
                            font.pixelSize: 10
                        }
                    }

                    HoverHandler {
                        id: tileHover
                        cursorShape: tileDrag.active ? Qt.ClosedHandCursor
                                                     : Qt.OpenHandCursor
                    }
                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: editorProject.activeCharacterEditor = tile.index
                    }
                    DragHandler {
                        id: tileDrag
                        target: null
                        acceptedButtons: Qt.LeftButton
                        property int startingX
                        property int startingY

                        onActiveChanged: {
                            if (active) {
                                startingX = tile.slotData.tileX
                                startingY = tile.slotData.tileY
                                editorProject.activeCharacterEditor = tile.index
                            }
                        }
                        onTranslationChanged: {
                            if (!active)
                                return
                            editorProject.moveCharacterTile(
                                tile.index,
                                startingX + translation.x / root.zoomScale,
                                startingY + translation.y / root.zoomScale)
                        }
                    }
                }
            }
        }
    }
}

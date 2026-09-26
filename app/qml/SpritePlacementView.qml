pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property real zoomScale
    readonly property var placements: editorProject.activeSpritePlacements

    function paletteColor(index) {
        const colors = editorProject.characterPaletteColors
        if (index < 0 || index >= colors.length)
            return "#000000"
        return index === 0 ? "transparent" : colors[index]
    }

    Flickable {
        id: placementFlick
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: Math.max(width, placementBox.width + 8)
        contentHeight: Math.max(height, placementBox.height + 8)
        ScrollBar.horizontal: ScrollBar {}
        ScrollBar.vertical: ScrollBar {}

        Rectangle {
            id: placementBox
            objectName: "spritePlacementBox"
            x: 4
            y: 4
            width: editorProject.placementWidth * root.zoomScale
            height: editorProject.placementHeight * root.zoomScale
            color: "#090b0e"
            border.width: 1
            border.color: "#71808f"
            clip: true

            Image {
                anchors.fill: parent
                source: imageInput.convertedPreview
                fillMode: Image.Stretch
                opacity: 0.38
                visible: imageInput.hasConversion
            }

            Repeater {
                model: Math.floor(editorProject.placementWidth / 8) + 1
                Rectangle {
                    required property int index
                    x: Math.min(placementBox.width - 1,
                                index * 8 * root.zoomScale)
                    width: 1
                    height: placementBox.height
                    color: index % 4 === 0 ? "#394854" : "#26313a"
                }
            }
            Repeater {
                model: Math.floor(editorProject.placementHeight / 8) + 1
                Rectangle {
                    required property int index
                    y: Math.min(placementBox.height - 1,
                                index * 8 * root.zoomScale)
                    width: placementBox.width
                    height: 1
                    color: index % 4 === 0 ? "#394854" : "#26313a"
                }
            }

            Repeater {
                model: root.placements

                delegate: Item {
                    id: spriteMarker
                    required property int index
                    required property var modelData
                    readonly property int spriteSize: modelData.size
                    readonly property var pixels: {
                        const revision = editorProject.spriteRevision
                        return editorProject.spritePatternPixels(
                            editorProject.activeSpriteSet, index, spriteSize)
                    }
                    readonly property bool active: index === editorProject.activeSprite
                    x: modelData.x * root.zoomScale
                    y: modelData.y * root.zoomScale
                    width: spriteSize * root.zoomScale
                    height: spriteSize * root.zoomScale
                    z: active ? 1000 : index + 10
                    visible: modelData.visible

                    Repeater {
                        model: spriteMarker.spriteSize * spriteMarker.spriteSize
                        Rectangle {
                            required property int index
                            readonly property int value:
                                spriteMarker.pixels.length > index
                                ? spriteMarker.pixels[index] : 0
                            x: (index % spriteMarker.spriteSize) * root.zoomScale
                            y: Math.floor(index / spriteMarker.spriteSize)
                               * root.zoomScale
                            width: root.zoomScale
                            height: root.zoomScale
                            color: value === 0 ? "transparent"
                                  : editorProject.editScope === 0
                                    ? root.paletteColor(spriteMarker.modelData.color)
                                    : root.paletteColor(value)
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.width: spriteMarker.active ? 2 : 1
                        border.color: spriteMarker.active ? "white" : "#62c5ff"
                    }

                    Rectangle {
                        visible: spriteHover.hovered
                        z: 2000
                        y: spriteMarker.height + 2
                        width: spriteLabel.implicitWidth + 8
                        height: 18
                        color: "#202a33"
                        border.width: 1
                        border.color: spriteMarker.active ? "white" : "#73808c"
                        radius: 2
                        Label {
                            id: spriteLabel
                            anchors.centerIn: parent
                            text: qsTr("Sprite %1 · %2×%2 · %3,%4")
                                  .arg(spriteMarker.index)
                                  .arg(spriteMarker.spriteSize)
                                  .arg(spriteMarker.modelData.x)
                                  .arg(spriteMarker.modelData.y)
                            font.pixelSize: 10
                        }
                    }

                    HoverHandler {
                        id: spriteHover
                        cursorShape: spriteDrag.active
                                     ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    }
                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: {
                            editorProject.activeSprite = spriteMarker.index
                            editorProject.activeSpriteSize = spriteMarker.spriteSize
                        }
                    }
                    DragHandler {
                        id: spriteDrag
                        target: null
                        acceptedButtons: Qt.LeftButton
                        property int startingX
                        property int startingY

                        onActiveChanged: {
                            if (active) {
                                startingX = spriteMarker.modelData.x
                                startingY = spriteMarker.modelData.y
                                editorProject.activeSprite = spriteMarker.index
                                editorProject.activeSpriteSize = spriteMarker.spriteSize
                            }
                        }
                        onTranslationChanged: {
                            if (!active)
                                return
                            editorProject.moveSprite(
                                spriteMarker.index,
                                Math.round(startingX + translation.x / root.zoomScale),
                                Math.round(startingY + translation.y / root.zoomScale))
                        }
                    }
                }
            }
        }
    }
}

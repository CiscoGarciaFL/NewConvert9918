import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    Item {
        anchors.fill: parent

        GridView {
            id: spriteGrid
            objectName: "spritePatternGrid"
            anchors.fill: parent
            visible: !editorProject.spritePlacementMode
            model: 32
            clip: true
            cellWidth: Math.max(56, Math.floor(width / 8))
            cellHeight: cellWidth
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                required property int index
                width: spriteGrid.cellWidth - 4
                height: spriteGrid.cellHeight - 4
                color: index === editorProject.activeSprite ? "#315f82" : "#1b2229"
                border.width: index === editorProject.activeSprite ? 2 : 1
                border.color: index === editorProject.activeSprite
                              ? "#8fd0ff" : "#53606d"
                radius: 3

                Text {
                    anchors.centerIn: parent
                    text: parent.index
                    color: "#d8dde3"
                    font.pixelSize: 17
                }
                TapHandler {
                    onTapped: editorProject.activeSprite = parent.index
                }
            }
        }

        Item {
            objectName: "spritePlacementWorkspace"
            anchors.fill: parent
            visible: editorProject.spritePlacementMode

            Rectangle {
                id: placementBox
                objectName: "spritePlacementBox"
                anchors.centerIn: parent
                width: Math.min(parent.width - 28,
                                (parent.height - 28) * editorProject.placementWidth
                                / editorProject.placementHeight)
                height: width * editorProject.placementHeight
                        / editorProject.placementWidth
                color: "#090b0e"
                border.width: 2
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
                    model: editorProject.activeSpritePlacements

                    Rectangle {
                        id: spriteMarker
                        required property var modelData
                        readonly property real scaleX: placementBox.width
                                                       / editorProject.placementWidth
                        readonly property real scaleY: placementBox.height
                                                       / editorProject.placementHeight
                        x: modelData.x * scaleX
                        y: modelData.y * scaleY
                        width: Math.max(8, 16 * scaleX)
                        height: Math.max(8, 16 * scaleY)
                        color: modelData.index === editorProject.activeSprite
                               ? "#5578b8ff" : "#302bd2a4"
                        border.width: modelData.index === editorProject.activeSprite ? 2 : 1
                        border.color: modelData.index === editorProject.activeSprite
                                      ? "white" : "#a7ddff"
                        visible: modelData.visible

                        Text {
                            anchors.centerIn: parent
                            text: spriteMarker.modelData.index
                            color: "white"
                            font.pixelSize: 13
                        }
                        TapHandler {
                            onTapped: editorProject.activeSprite = spriteMarker.modelData.index
                        }
                        DragHandler {
                            id: spriteDrag
                            onActiveChanged: {
                                if (!active) {
                                    editorProject.moveSprite(
                                        spriteMarker.modelData.index,
                                        Math.round(spriteMarker.x / spriteMarker.scaleX),
                                        Math.round(spriteMarker.y / spriteMarker.scaleY))
                                }
                            }
                        }
                    }
                }
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                text: imageInput.hasConversion
                      ? qsTr("Screen Image reference · drag sprite boxes to place them")
                      : qsTr("Create a Screen Image to use it as the placement reference")
                color: "#c2c8cf"
            }
        }
    }
}

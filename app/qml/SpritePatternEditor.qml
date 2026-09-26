pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    objectName: "spritePatternEditor"

    required property int editorIndex
    required property int editorCount
    required property bool loaded
    required property int setIndex
    required property int spriteIndex
    required property int spriteSize
    required property int spriteColorIndex
    required property int drawingTool
    required property bool active
    readonly property real cellSize: 16
    readonly property real gridSize: cellSize * spriteSize
    readonly property var pixels: {
        const revision = editorProject.spriteRevision
        if (!loaded)
            return []
        return editorProject.spritePatternPixels(setIndex, spriteIndex, spriteSize)
    }
    readonly property var paletteColors: editorProject.characterPaletteColors
    readonly property bool pixelEditingEnabled:
        loaded && !editorProject.spritePanActive

    signal selected()
    signal moveRequested(int targetIndex)
    signal removeRequested()

    implicitWidth: gridSize + 96
    implicitHeight: gridSize + 60
    color: "#13191f"
    border.width: active ? 2 : 1
    border.color: active ? palette.highlight : "#46515d"
    radius: 4

    function hexRow(row) {
        let value = 0
        for (let column = 0; column < spriteSize; ++column) {
            if (loaded && pixels.length > row * spriteSize + column
                    && pixels[row * spriteSize + column] !== 0)
                value += Math.pow(2, spriteSize - 1 - column)
        }
        return value.toString(16).toUpperCase().padStart(spriteSize / 4, "0")
    }

    function pixelColor(value) {
        if (value === 0)
            return "#20272e"
        if (editorProject.editScope === 0)
            return paletteColors[spriteColorIndex]
        return paletteColors[Math.min(paletteColors.length - 1, value)]
    }

    function paintAt(localX, localY) {
        root.selected()
        if (!pixelEditingEnabled)
            return
        const column = Math.max(0, Math.min(spriteSize - 1,
                                            Math.floor(localX / cellSize)))
        const row = Math.max(0, Math.min(spriteSize - 1,
                                         Math.floor(localY / cellSize)))
        editorProject.paintSpritePixel(setIndex, spriteIndex, spriteSize,
                                       row, column, drawingTool === 1)
    }

    Menu {
        id: orderMenu
        parent: patternLabel

        MenuItem {
            text: qsTr("Move left")
            enabled: root.editorIndex > 0
            onTriggered: root.moveRequested(root.editorIndex - 1)
        }
        MenuItem {
            text: qsTr("Move right")
            enabled: root.editorIndex + 1 < root.editorCount
            onTriggered: root.moveRequested(root.editorIndex + 1)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Move to first")
            enabled: root.editorIndex > 0
            onTriggered: root.moveRequested(0)
        }
        MenuItem {
            text: qsTr("Move to last")
            enabled: root.editorIndex + 1 < root.editorCount
            onTriggered: root.moveRequested(root.editorCount - 1)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Remove editor")
            enabled: root.editorCount > 1
            onTriggered: root.removeRequested()
        }
    }

    Row {
        id: dataRow
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.top: parent.top
        anchors.topMargin: 8
        height: root.gridSize
        spacing: 2

        Item {
            width: 10
            height: root.gridSize
            Label {
                anchors.centerIn: parent
                text: qsTr("Mask")
                color: palette.placeholderText
                font.pixelSize: 10
                rotation: -90
            }
        }

        Column {
            width: 38
            Repeater {
                model: root.spriteSize
                Label {
                    required property int index
                    width: 38
                    height: root.cellSize
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    text: root.hexRow(index)
                    color: "#c2c8cf"
                    font.family: "monospace"
                    font.pixelSize: root.spriteSize === 16 ? 8 : 11
                }
            }
        }

        Item {
            id: pixelGrid
            objectName: "spritePatternPixelGrid"
            width: root.gridSize
            height: root.gridSize

            Repeater {
                model: root.spriteSize * root.spriteSize
                Rectangle {
                    required property int index
                    readonly property int pixelRow: Math.floor(index / root.spriteSize)
                    readonly property int pixelColumn: index % root.spriteSize
                    x: pixelColumn * root.cellSize
                    y: pixelRow * root.cellSize
                    width: root.cellSize
                    height: root.cellSize
                    color: !root.loaded || root.pixels.length <= index
                           ? "#20272e" : root.pixelColor(root.pixels[index])
                }
            }

            Repeater {
                model: root.spriteSize + 1
                Rectangle {
                    required property int index
                    x: Math.max(0, Math.min(pixelGrid.width - width,
                                           index * root.cellSize - width / 2))
                    width: index > 0 && index < root.spriteSize
                           && index % 8 === 0 ? 3 : 1
                    height: pixelGrid.height
                    color: width > 1 ? "#d8dde3" : "#68737f"
                }
            }
            Repeater {
                model: root.spriteSize + 1
                Rectangle {
                    required property int index
                    y: Math.max(0, Math.min(pixelGrid.height - height,
                                           index * root.cellSize - height / 2))
                    width: pixelGrid.width
                    height: index > 0 && index < root.spriteSize
                            && index % 8 === 0 ? 3 : 1
                    color: height > 1 ? "#d8dde3" : "#68737f"
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: root.pixelEditingEnabled
                             ? Qt.CrossCursor : Qt.PointingHandCursor
                preventStealing: true
                onPressed: mouse => {
                    root.selected()
                    if (root.pixelEditingEnabled) {
                        editorProject.beginSpriteEdit(root.setIndex,
                                                      root.spriteIndex,
                                                      root.spriteSize)
                        root.paintAt(mouse.x, mouse.y)
                    }
                }
                onPositionChanged: mouse => {
                    if (pressed && root.pixelEditingEnabled)
                        root.paintAt(mouse.x, mouse.y)
                }
                onReleased: editorProject.endSpriteEdit()
                onCanceled: editorProject.endSpriteEdit()
            }
        }
    }

    ToolButton {
        id: patternLabel
        objectName: "spritePatternEditorLabel"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        x: dataRow.x + pixelGrid.x + (pixelGrid.width - width) / 2
        implicitWidth: 104
        implicitHeight: 24
        checkable: true
        checked: root.active
        text: root.loaded
              ? qsTr("%1x%1 Sprite %2")
                    .arg(root.spriteSize)
                    .arg(root.spriteIndex.toString().padStart(2, "0"))
              : qsTr("Empty")
        Accessible.name: root.loaded
                         ? qsTr("Select %1 by %1 sprite %2")
                               .arg(root.spriteSize).arg(root.spriteIndex)
                         : qsTr("Select empty sprite editor")
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Select this editor; right-click or hold for ordering options")
        onClicked: root.selected()
        onPressAndHold: {
            root.selected()
            orderMenu.open()
        }

        background: Rectangle {
            radius: 2
            color: root.active ? patternLabel.palette.highlight
                               : patternLabel.palette.button
            border.width: 1
            border.color: root.active ? patternLabel.palette.highlight
                                      : patternLabel.palette.mid
        }
        contentItem: Text {
            text: patternLabel.text
            font: patternLabel.font
            color: root.active ? patternLabel.palette.highlightedText
                               : patternLabel.palette.buttonText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: {
                root.selected()
                orderMenu.open()
            }
        }
    }

    Rectangle {
        id: preview
        objectName: "spritePatternPreview"
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        width: 34
        height: 34
        color: "#20272e"
        border.width: 1
        border.color: root.active ? palette.highlight : "#68737f"

        Item {
            anchors.centerIn: parent
            width: root.spriteSize
            height: root.spriteSize
            Repeater {
                model: root.spriteSize * root.spriteSize
                Rectangle {
                    required property int index
                    x: index % root.spriteSize
                    y: Math.floor(index / root.spriteSize)
                    width: 1
                    height: 1
                    color: !root.loaded || root.pixels.length <= index
                           ? "#20272e" : root.pixelColor(root.pixels[index])
                }
            }
        }
        TapHandler { onTapped: root.selected() }
    }
}

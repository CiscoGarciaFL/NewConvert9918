pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Reusable 8x8 pattern editor. Explicit slot, set, and pattern inputs allow
// multiple independent editors to share one adaptive tray.
Rectangle {
    id: root

    required property int editorIndex
    required property int editorCount
    required property bool loaded
    required property int patternSetIndex
    required property int patternIndex
    required property int drawingTool // 1 = pencil, 2 = eraser
    required property bool active
    readonly property real cellSize: 16
    readonly property real gridSize: cellSize * 8
    property int previewScaleIndex: 3
    readonly property int previewScale: previewScaleIndex + 1
    readonly property var rows: {
        const revision = editorProject.characterRevision
        if (!loaded)
            return []
        return editorProject.characterPatternRows(patternSetIndex, patternIndex)
    }
    readonly property var paletteColors: editorProject.characterPaletteColors
    readonly property bool pixelEditingEnabled:
        loaded && !editorProject.characterPanActive

    signal selected()
    signal moveRequested(int targetIndex)
    signal removeRequested()

    implicitWidth: 224
    implicitHeight: 188
    color: "#13191f"
    border.width: active ? 2 : 1
    border.color: active ? palette.highlight : "#46515d"
    radius: 4

    function hexByte(value) {
        return Number(value).toString(16).toUpperCase().padStart(2, "0")
    }

    function paletteColor(index) {
        if (index < 0 || index >= paletteColors.length)
            return "#000000"
        if (index === 0)
            return "#303842"
        return paletteColors[index]
    }

    function paintAt(localX, localY) {
        root.selected()
        if (!pixelEditingEnabled)
            return
        const column = Math.max(0, Math.min(7, Math.floor(localX / cellSize)))
        const row = Math.max(0, Math.min(7, Math.floor(localY / cellSize)))
        editorProject.paintCharacterPixel(patternSetIndex, patternIndex,
                                          row, column, drawingTool === 1)
    }

    function increasePreviewScale() {
        previewScaleIndex = Math.min(3, previewScaleIndex + 1)
    }

    function decreasePreviewScale() {
        previewScaleIndex = Math.max(0, previewScaleIndex - 1)
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
        id: patternDataRow
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.top: parent.top
        anchors.topMargin: 8
        height: root.gridSize
        spacing: 0

        Item {
            width: 10
            height: root.gridSize

            Label {
                anchors.centerIn: parent
                text: qsTr("Bits")
                color: palette.placeholderText
                font.pixelSize: 10
                rotation: -90
            }
        }

        Column {
            width: 28
            spacing: 0
            Repeater {
                model: 8
                Label {
                    required property int index
                    width: 28
                    height: root.cellSize
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignRight
                    text: root.loaded && root.rows.length === 8
                          ? root.hexByte(root.rows[index].pattern) : "--"
                    color: "#c2c8cf"
                    font.family: "monospace"
                    font.pixelSize: 11
                }
            }
        }

        Item { width: 2; height: 1 }

        Item {
            id: pixelGrid
            objectName: "characterPatternPixelGrid"
            width: root.gridSize
            height: root.gridSize

            Repeater {
                model: 64
                Rectangle {
                    required property int index
                    readonly property int patternRow: Math.floor(index / 8)
                    readonly property int patternColumn: index % 8
                    readonly property bool foregroundPixel:
                        root.loaded && root.rows.length === 8
                        && (root.rows[patternRow].pattern
                            & (0x80 >> patternColumn)) !== 0
                    x: patternColumn * root.cellSize
                    y: patternRow * root.cellSize
                    width: root.cellSize
                    height: root.cellSize
                    color: !root.loaded || root.rows.length !== 8 ? "#20272e"
                           : root.paletteColor(foregroundPixel
                                               ? root.rows[patternRow].foreground
                                               : root.rows[patternRow].background)
                }
            }

            Repeater {
                model: 9
                Rectangle {
                    required property int index
                    x: Math.max(0, Math.min(pixelGrid.width - width,
                                          index * root.cellSize - width / 2))
                    width: index === 4 ? 3 : 1
                    height: pixelGrid.height
                    color: index === 4 ? "#d8dde3" : "#68737f"
                }
            }
            Repeater {
                model: 9
                Rectangle {
                    required property int index
                    y: Math.max(0, Math.min(pixelGrid.height - height,
                                          index * root.cellSize - height / 2))
                    width: pixelGrid.width
                    height: 1
                    color: "#68737f"
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
                        editorProject.beginCharacterEdit(
                            root.patternSetIndex, root.patternIndex)
                        root.paintAt(mouse.x, mouse.y)
                    }
                }
                onPositionChanged: mouse => {
                    if (pressed && root.pixelEditingEnabled)
                        root.paintAt(mouse.x, mouse.y)
                }
                onReleased: editorProject.endCharacterEdit()
                onCanceled: editorProject.endCharacterEdit()
            }
        }

        Item { width: 2; height: 1 }

        Column {
            id: colorValueColumn
            width: 28
            spacing: 0
            Repeater {
                model: 8
                Label {
                    required property int index
                    width: 28
                    height: root.cellSize
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignLeft
                    text: root.loaded && root.rows.length === 8
                          ? root.hexByte(root.rows[index].color) : "--"
                    color: "#c2c8cf"
                    font.family: "monospace"
                    font.pixelSize: 11
                }
            }
        }

        Item {
            width: 10
            height: root.gridSize

            Label {
                anchors.centerIn: parent
                text: qsTr("Color")
                color: palette.placeholderText
                font.pixelSize: 10
                rotation: -90
            }
        }
    }

    ToolButton {
        id: patternLabel
        objectName: "characterPatternEditorLabel"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        x: patternDataRow.x + pixelGrid.x
           + (pixelGrid.width - width) / 2
        implicitWidth: 88
        implicitHeight: 24
        checkable: true
        checked: root.active
        text: root.loaded
              ? qsTr("Pattern %1").arg(root.hexByte(root.patternIndex))
              : qsTr("Empty")
        Accessible.name: root.loaded
                         ? qsTr("Select editor for pattern %1")
                               .arg(root.hexByte(root.patternIndex))
                         : qsTr("Select empty pattern editor")
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
        id: characterPreview
        objectName: "characterPatternPreview"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        x: patternLabel.x + patternLabel.width + 1 + 34 - width
        width: root.previewScale * 8 + 2
        height: width
        color: "#20272e"
        border.width: 1
        border.color: root.active ? palette.highlight : "#68737f"
        Accessible.name: qsTr("Pattern preview at %1 times size")
                         .arg(root.previewScale)

        Item {
            anchors.fill: parent
            anchors.margins: 1

            Repeater {
                model: 64

                Rectangle {
                    required property int index
                    readonly property int patternRow: Math.floor(index / 8)
                    readonly property int patternColumn: index % 8
                    readonly property bool foregroundPixel:
                        root.loaded && root.rows.length === 8
                        && (root.rows[patternRow].pattern
                            & (0x80 >> patternColumn)) !== 0
                    x: patternColumn * root.previewScale
                    y: patternRow * root.previewScale
                    width: root.previewScale
                    height: root.previewScale
                    color: !root.loaded || root.rows.length !== 8 ? "#20272e"
                           : root.paletteColor(foregroundPixel
                                               ? root.rows[patternRow].foreground
                                               : root.rows[patternRow].background)
                }
            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: root.selected()
        }
        HoverHandler { id: previewHover }
        ToolTip.visible: previewHover.hovered
        ToolTip.text: qsTr("Character preview · %1×").arg(root.previewScale)
    }

    ColumnLayout {
        id: previewScaleButtons
        objectName: "characterPatternPreviewScaleButtons"
        anchors.left: characterPreview.right
        anchors.leftMargin: 2
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        width: 18
        height: 26
        spacing: 0

        ToolButton {
            objectName: "characterPatternPreviewZoomInButton"
            Layout.fillWidth: true
            Layout.preferredHeight: 13
            leftPadding: 1
            rightPadding: 1
            topPadding: 0
            bottomPadding: 0
            text: "+"
            font.pixelSize: 11
            enabled: root.previewScaleIndex < 3
            Accessible.name: qsTr("Increase character preview size")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Preview at %1×")
                          .arg(Math.min(4, root.previewScale + 1))
            onClicked: root.increasePreviewScale()
        }
        ToolButton {
            objectName: "characterPatternPreviewZoomOutButton"
            Layout.fillWidth: true
            Layout.preferredHeight: 13
            leftPadding: 1
            rightPadding: 1
            topPadding: 0
            bottomPadding: 0
            text: "−"
            font.pixelSize: 11
            enabled: root.previewScaleIndex > 0
            Accessible.name: qsTr("Decrease character preview size")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Preview at %1×")
                          .arg(Math.max(1, root.previewScale - 1))
            onClicked: root.decreasePreviewScale()
        }
    }
}

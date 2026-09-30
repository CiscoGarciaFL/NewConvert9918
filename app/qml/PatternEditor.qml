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
    required property int drawingTool // 1 pencil, 2 eraser, 3 line, 4 K-Line, 5 Rays
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
    property bool kLineActive: false
    property int kLineRow: 0
    property int kLineColumn: 0
    property bool raysActive: false
    property int raysOriginRow: 0
    property int raysOriginColumn: 0
    property var linePreviewCells: []

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

    function cellAt(localX, localY) {
        return Qt.point(
            Math.max(0, Math.min(7, Math.floor(localX / cellSize))),
            Math.max(0, Math.min(7, Math.floor(localY / cellSize))))
    }

    function constrainedCell(fromColumn, fromRow, column, row, locked) {
        if (!locked)
            return Qt.point(column, row)
        if (Math.abs(column - fromColumn) >= Math.abs(row - fromRow))
            return Qt.point(column, fromRow)
        return Qt.point(fromColumn, row)
    }

    function lineCells(fromColumn, fromRow, toColumn, toRow) {
        const result = []
        let x = fromColumn
        let y = fromRow
        const deltaX = Math.abs(toColumn - fromColumn)
        const stepX = fromColumn < toColumn ? 1 : -1
        const deltaY = -Math.abs(toRow - fromRow)
        const stepY = fromRow < toRow ? 1 : -1
        let error = deltaX + deltaY
        while (true) {
            result.push(y * 8 + x)
            if (x === toColumn && y === toRow)
                break
            const twiceError = error * 2
            if (twiceError >= deltaY) {
                error += deltaY
                x += stepX
            }
            if (twiceError <= deltaX) {
                error += deltaX
                y += stepY
            }
        }
        return result
    }

    function updateLinePreview(fromColumn, fromRow, localX, localY, locked) {
        const cell = cellAt(localX, localY)
        const end = constrainedCell(fromColumn, fromRow, cell.x, cell.y, locked)
        linePreviewCells = lineCells(fromColumn, fromRow, end.x, end.y)
        return end
    }

    function finishMultiLine() {
        if (!kLineActive && !raysActive)
            return
        editorProject.endCharacterEdit()
        kLineActive = false
        raysActive = false
        linePreviewCells = []
    }

    function finishKLine() {
        finishMultiLine()
    }

    onDrawingToolChanged: {
        if ((kLineActive && drawingTool !== 4)
                || (raysActive && drawingTool !== 5))
            finishMultiLine()
    }
    onActiveChanged: {
        if (!active)
            finishMultiLine()
    }
    Component.onDestruction: finishMultiLine()

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

            Repeater {
                model: root.linePreviewCells
                Rectangle {
                    required property int modelData
                    x: (modelData % 8) * root.cellSize + 2
                    y: Math.floor(modelData / 8) * root.cellSize + 2
                    width: root.cellSize - 4
                    height: root.cellSize - 4
                    color: "#55ffffff"
                    border.width: 2
                    border.color: root.palette.highlight
                }
            }

            MouseArea {
                id: drawingArea
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: root.pixelEditingEnabled
                             ? Qt.CrossCursor : Qt.PointingHandCursor
                preventStealing: true
                hoverEnabled: true
                focus: root.kLineActive || root.raysActive
                property int dragTool: 0
                property int startRow: 0
                property int startColumn: 0
                Keys.onEscapePressed: event => {
                    if (root.kLineActive || root.raysActive) {
                        root.finishMultiLine()
                        event.accepted = true
                    }
                }
                onPressed: mouse => {
                    root.selected()
                    dragTool = root.drawingTool
                    if (!root.pixelEditingEnabled)
                        return
                    const cell = root.cellAt(mouse.x, mouse.y)
                    if (dragTool <= 2) {
                        editorProject.beginCharacterEdit(
                            root.patternSetIndex, root.patternIndex)
                        root.paintAt(mouse.x, mouse.y)
                    } else if (dragTool === 3) {
                        startColumn = cell.x
                        startRow = cell.y
                        root.updateLinePreview(startColumn, startRow,
                                               mouse.x, mouse.y, false)
                    } else if (dragTool === 4 || dragTool === 5) {
                        forceActiveFocus()
                        const active = dragTool === 4
                            ? root.kLineActive : root.raysActive
                        if (!active) {
                            editorProject.beginCharacterEdit(
                                root.patternSetIndex, root.patternIndex)
                            editorProject.paintCharacterPixel(
                                root.patternSetIndex, root.patternIndex,
                                cell.y, cell.x, true)
                            if (dragTool === 4) {
                                root.kLineColumn = cell.x
                                root.kLineRow = cell.y
                                root.kLineActive = true
                            } else {
                                root.raysOriginColumn = cell.x
                                root.raysOriginRow = cell.y
                                root.raysActive = true
                            }
                        } else {
                            const fromColumn = dragTool === 4
                                ? root.kLineColumn : root.raysOriginColumn
                            const fromRow = dragTool === 4
                                ? root.kLineRow : root.raysOriginRow
                            const end = root.constrainedCell(
                                fromColumn, fromRow,
                                cell.x, cell.y,
                                (mouse.modifiers & Qt.ShiftModifier) !== 0)
                            editorProject.drawCharacterLine(
                                root.patternSetIndex, root.patternIndex,
                                fromRow, fromColumn,
                                end.y, end.x, true)
                            if (dragTool === 4) {
                                root.kLineColumn = end.x
                                root.kLineRow = end.y
                            }
                        }
                        root.linePreviewCells = []
                    }
                }
                onPositionChanged: mouse => {
                    if ((root.kLineActive && root.drawingTool === 4)
                            || (root.raysActive && root.drawingTool === 5)) {
                        const fromColumn = root.drawingTool === 4
                            ? root.kLineColumn : root.raysOriginColumn
                        const fromRow = root.drawingTool === 4
                            ? root.kLineRow : root.raysOriginRow
                        root.updateLinePreview(
                            fromColumn, fromRow,
                            mouse.x, mouse.y,
                            (mouse.modifiers & Qt.ShiftModifier) !== 0)
                    } else if (pressed && root.pixelEditingEnabled) {
                        if (dragTool <= 2) {
                            root.paintAt(mouse.x, mouse.y)
                        } else if (dragTool === 3) {
                            root.updateLinePreview(
                                startColumn, startRow, mouse.x, mouse.y,
                                (mouse.modifiers & Qt.ShiftModifier) !== 0)
                        }
                    }
                }
                onReleased: mouse => {
                    if (dragTool <= 2) {
                        editorProject.endCharacterEdit()
                    } else if (dragTool === 3 && root.pixelEditingEnabled) {
                        const end = root.updateLinePreview(
                            startColumn, startRow, mouse.x, mouse.y,
                            (mouse.modifiers & Qt.ShiftModifier) !== 0)
                        editorProject.drawCharacterLine(
                            root.patternSetIndex, root.patternIndex,
                            startRow, startColumn, end.y, end.x, true)
                        root.linePreviewCells = []
                    }
                    dragTool = 0
                }
                onCanceled: {
                    if (dragTool <= 2)
                        editorProject.endCharacterEdit()
                    else if (dragTool === 4 || dragTool === 5)
                        root.finishMultiLine()
                    root.linePreviewCells = []
                    dragTool = 0
                }
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

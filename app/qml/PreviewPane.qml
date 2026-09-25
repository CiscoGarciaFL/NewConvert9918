import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ThemedFrame {
    id: root

    required property string title
    property url imageSource
    property string emptyText: qsTr("No image")
    property string details
    property bool busy: false
    property bool cropOverlay: false
    property bool acceptDrops: false
    property bool showTitle: true
    property bool sourceTools: false
    property bool convertedTools: false
    property int activeColor: 1 // 0 = foreground, 1 = background
    property bool pickingColor: false
    property int drawingTool: 0 // 0 = none, 1 = pencil, 2 = eraser, 3 = ellipse, 4 = rectangle
    property int drawingDiameter: 1
    property bool hardDrawingEdges: false
    property bool fillDrawingShapes: false
    property Component upperToolbarContent
    property Component workspaceContent
    property Component lowerToolbarContent
    signal fileDropped(url fileUrl)
    signal colorPointPicked(real normalizedX, real normalizedY, bool foreground)

    property bool fitToView: true
    property real manualZoom: 1.0
    readonly property real fitZoom: {
        if (preview.sourceSize.width <= 0 || preview.sourceSize.height <= 0)
            return 1.0
        return Math.min(viewport.width / preview.sourceSize.width,
                        viewport.height / preview.sourceSize.height)
    }
    readonly property real effectiveZoom: fitToView ? fitZoom : manualZoom

    function zoomBy(factor) {
        const startingZoom = effectiveZoom
        manualZoom = Math.max(0.125, Math.min(16, startingZoom * factor))
        fitToView = false
    }

    function chooseActiveColor(color) {
        if (root.activeColor === 0)
            imageInput.foregroundColor = color
        else
            imageInput.backgroundColor = color
        backgroundPalettePopup.close()
    }

    Popup {
        id: backgroundPalettePopup
        objectName: root.objectName + "ColorPickerPopup"
        enabled: imageInput.hasImage
        parent: root
        x: Math.max(0, Math.min(root.width - width,
                               colorControl.mapToItem(root, 0, 0).x))
        y: Math.max(0, colorControl.mapToItem(root, 0, 0).y - height - 4)
        width: 352
        height: 330
        padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onEnabledChanged: {
            if (!enabled)
                close()
        }

        background: Rectangle {
            color: backgroundPalettePopup.palette.window
            border.width: 1
            border.color: backgroundPalettePopup.palette.windowText
            radius: 3
        }

        contentItem: ColumnLayout {
            spacing: 6

            TabBar {
                id: colorPickerTabs
                objectName: root.objectName + "ColorPickerTabs"
                Layout.fillWidth: true
                TabButton { text: qsTr("Spectrum") }
                TabButton { text: qsTr("Standard") }
                TabButton { text: qsTr("Used by image") }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: colorPickerTabs.currentIndex

                ColumnLayout {
                    spacing: 5
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("PC color spectrum") }
                        ComboBox {
                            id: spectrumSize
                            objectName: root.objectName + "SpectrumSizeComboBox"
                            Layout.preferredWidth: 90
                            model: [16, 32, 64]
                            Accessible.name: qsTr("Spectrum color count")
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Black and white are first")
                            color: palette.placeholderText
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                    GridView {
                        objectName: root.objectName + "SpectrumColorGrid"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        cellWidth: 38
                        cellHeight: 38
                        clip: true
                        model: spectrumSize.currentIndex === 0
                               ? imageInput.sourceSpectrum16Colors.length
                               : spectrumSize.currentIndex === 1
                                 ? imageInput.sourceSpectrum32Colors.length
                                 : imageInput.sourceSpectrum64Colors.length
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            objectName: "spectrumColorSwatch"
                            property color swatchColor: spectrumSize.currentIndex === 0
                                ? imageInput.sourceSpectrum16Colors[index]
                                : spectrumSize.currentIndex === 1
                                  ? imageInput.sourceSpectrum32Colors[index]
                                  : imageInput.sourceSpectrum64Colors[index]
                            width: 32
                            height: 32
                            radius: 3
                            color: swatchColor
                            border.width: colorHover.hovered ? 2 : 1
                            border.color: colorHover.hovered ? palette.highlight : "#707780"
                            Accessible.name: qsTr("Select %1").arg(String(swatchColor))
                            HoverHandler { id: colorHover }
                            TapHandler {
                                acceptedButtons: Qt.LeftButton
                                onTapped: root.chooseActiveColor(swatchColor)
                            }
                            ToolTip.visible: colorHover.hovered
                            ToolTip.text: String(swatchColor)
                        }
                    }
                }

                GridView {
                    objectName: root.objectName + "StandardColorGrid"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    cellWidth: 38
                    cellHeight: 38
                    clip: true
                    model: imageInput.sourceSwatchColors.length
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        objectName: "standardColorSwatch"
                        property color swatchColor: imageInput.sourceSwatchColors[index]
                        width: 32
                        height: 32
                        radius: 3
                        color: swatchColor
                        border.width: colorHover.hovered ? 2 : 1
                        border.color: colorHover.hovered ? palette.highlight : "#707780"
                        Accessible.name: qsTr("Select %1").arg(String(swatchColor))
                        HoverHandler { id: colorHover }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: root.chooseActiveColor(swatchColor)
                        }
                        ToolTip.visible: colorHover.hovered
                        ToolTip.text: String(swatchColor)
                    }
                }

                ColumnLayout {
                    spacing: 5
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Colors found in the source image")
                        color: palette.placeholderText
                    }
                    GridView {
                        objectName: root.objectName + "UsedColorGrid"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        cellWidth: 38
                        cellHeight: 38
                        clip: true
                        model: imageInput.sourceUsedColors
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property color modelData
                            objectName: "usedColorSwatch"
                            property color swatchColor: modelData
                            width: 32
                            height: 32
                            radius: 3
                            color: swatchColor
                            border.width: colorHover.hovered ? 2 : 1
                            border.color: colorHover.hovered ? palette.highlight : "#707780"
                            Accessible.name: qsTr("Select %1").arg(String(swatchColor))
                            HoverHandler { id: colorHover }
                            TapHandler {
                                acceptedButtons: Qt.LeftButton
                                onTapped: root.chooseActiveColor(swatchColor)
                            }
                            ToolTip.visible: colorHover.hovered
                            ToolTip.text: String(swatchColor)
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            objectName: root.objectName + "Title"
            Layout.fillWidth: true
            visible: root.showTitle
            text: root.title
            font.weight: Font.DemiBold
        }

        Item {
            objectName: root.objectName + "DrawingToolbarSlot"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: width < 300 ? 53 : 26

            Loader {
                objectName: root.objectName + "CustomUpperToolbar"
                anchors.fill: parent
                sourceComponent: root.upperToolbarContent
                visible: sourceComponent !== null
            }

            Flow {
                objectName: root.objectName + "DrawingTools"
                anchors.fill: parent
                visible: root.sourceTools
                spacing: 1

                ToolButton {
                id: pencilButton
                objectName: root.objectName + "PencilButton"
                implicitWidth: 26
                implicitHeight: 26
                checkable: true
                checked: root.drawingTool === 1
                enabled: imageInput.hasImage
                Accessible.name: qsTr("Draw with the foreground color")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                contentItem: Canvas {
                    implicitWidth: 16
                    implicitHeight: 16
                    property color iconColor: pencilButton.enabled
                                              ? pencilButton.palette.buttonText
                                              : pencilButton.palette.mid
                    onIconColorChanged: requestPaint()
                    onPaint: {
                        const context = getContext("2d")
                        context.clearRect(0, 0, width, height)
                        context.strokeStyle = iconColor
                        context.fillStyle = iconColor
                        context.lineCap = "square"
                        context.lineWidth = 3
                        context.beginPath()
                        context.moveTo(4, 12)
                        context.lineTo(12, 4)
                        context.stroke()
                        context.beginPath()
                        context.moveTo(2, 14)
                        context.lineTo(5, 13)
                        context.lineTo(3, 11)
                        context.closePath()
                        context.fill()
                        context.lineWidth = 1
                        context.strokeRect(11, 2, 3, 3)
                    }
                }
                onClicked: {
                    root.drawingTool = 1
                    root.pickingColor = false
                }
                }
                ToolButton {
                id: eraserButton
                objectName: root.objectName + "EraserButton"
                implicitWidth: 26
                implicitHeight: 26
                checkable: true
                checked: root.drawingTool === 2
                enabled: imageInput.hasImage
                Accessible.name: qsTr("Erase with the background color")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                contentItem: Canvas {
                    implicitWidth: 16
                    implicitHeight: 16
                    property color iconColor: eraserButton.enabled
                                              ? eraserButton.palette.buttonText
                                              : eraserButton.palette.mid
                    onIconColorChanged: requestPaint()
                    onPaint: {
                        const context = getContext("2d")
                        context.clearRect(0, 0, width, height)
                        context.save()
                        context.translate(width / 2, height / 2)
                        context.rotate(-Math.PI / 4)
                        context.fillStyle = iconColor
                        context.fillRect(-5, -3, 10, 6)
                        context.strokeStyle = eraserButton.palette.button
                        context.lineWidth = 1
                        context.beginPath()
                        context.moveTo(1, -3)
                        context.lineTo(1, 3)
                        context.stroke()
                        context.restore()
                    }
                }
                onClicked: {
                    root.drawingTool = 2
                    root.pickingColor = false
                }
                }
                ToolButton {
                    id: ellipseButton
                    objectName: root.objectName + "EllipseButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    checkable: true
                    checked: root.drawingTool === 3
                    enabled: imageInput.hasImage
                    Accessible.name: qsTr("Draw an ellipse; hold Shift for a circle")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.drawingTool = 3
                        root.pickingColor = false
                    }

                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: ellipseButton.enabled
                                                  ? ellipseButton.palette.buttonText
                                                  : ellipseButton.palette.mid
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.lineWidth = 1.5
                            context.beginPath()
                            context.moveTo(8, 3)
                            context.bezierCurveTo(12, 3, 14, 5, 14, 8)
                            context.bezierCurveTo(14, 11, 12, 13, 8, 13)
                            context.bezierCurveTo(4, 13, 2, 11, 2, 8)
                            context.bezierCurveTo(2, 5, 4, 3, 8, 3)
                            context.closePath()
                            context.stroke()
                        }
                    }
                }
                ToolButton {
                    id: rectangleButton
                    objectName: root.objectName + "RectangleButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    checkable: true
                    checked: root.drawingTool === 4
                    enabled: imageInput.hasImage
                    Accessible.name: qsTr("Draw a rectangle; hold Shift for a square")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.drawingTool = 4
                        root.pickingColor = false
                    }

                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: rectangleButton.enabled
                                                  ? rectangleButton.palette.buttonText
                                                  : rectangleButton.palette.mid
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.lineWidth = 1.5
                            context.strokeRect(2, 3, 12, 10)
                        }
                    }
                }
                ToolButton {
                    id: hardEdgeButton
                    objectName: root.objectName + "HardEdgeButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    checkable: true
                    checked: root.hardDrawingEdges
                    enabled: imageInput.hasImage
                    Accessible.name: checked
                                     ? qsTr("Use hard drawing edges")
                                     : qsTr("Use soft drawing edges")
                    ToolTip.visible: hovered
                    ToolTip.text: checked
                                      ? qsTr("Hard edges: fully overwrite pixels without blending")
                                      : qsTr("Soft edges: blend the outer brush pixels")
                    onClicked: root.hardDrawingEdges = checked

                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        antialiasing: false
                        property color iconColor: hardEdgeButton.enabled
                                                  ? hardEdgeButton.palette.buttonText
                                                  : hardEdgeButton.palette.mid
                        property bool hardState: hardEdgeButton.checked
                        onIconColorChanged: requestPaint()
                        onHardStateChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.fillStyle = iconColor
                            context.lineCap = "square"
                            context.beginPath()
                            context.moveTo(3, 13)
                            context.lineTo(13, 3)
                            context.lineWidth = hardState ? 5 : 2
                            context.stroke()

                            if (!hardState) {
                                // Alternating fringe pixels suggest the partial
                                // coverage of a blended edge around a solid core.
                                const samples = [
                                    [0, 10], [6, 12], [4, 6],
                                    [10, 8], [8, 2], [14, 4]
                                ]
                                for (let index = 0; index < samples.length; ++index)
                                    context.fillRect(samples[index][0],
                                                     samples[index][1], 2, 2)
                            }
                        }
                    }
                }
                ToolButton {
                    id: shapeFillButton
                    objectName: root.objectName + "ShapeFillButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    checkable: true
                    checked: root.fillDrawingShapes
                    enabled: imageInput.hasImage
                    Accessible.name: checked
                                     ? qsTr("Fill shapes with the background color")
                                     : qsTr("Draw shapes without a fill")
                    ToolTip.visible: hovered
                    ToolTip.text: checked
                                      ? qsTr("Shape fill: background color")
                                      : qsTr("Shape fill: none")
                    onClicked: root.fillDrawingShapes = checked

                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: shapeFillButton.enabled
                                                  ? shapeFillButton.palette.buttonText
                                                  : shapeFillButton.palette.mid
                        property color fillColor: imageInput.backgroundColor
                        property bool filled: root.fillDrawingShapes
                        onIconColorChanged: requestPaint()
                        onFillColorChanged: requestPaint()
                        onFilledChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.lineWidth = 1.5
                            context.lineJoin = "round"

                            // A tilted paint bucket. Its body carries the active
                            // background color while shape filling is enabled.
                            context.save()
                            context.translate(7, 7)
                            context.rotate(-Math.PI / 4)
                            context.beginPath()
                            context.rect(-4, -3, 8, 7)
                            if (filled) {
                                context.fillStyle = fillColor
                                context.fill()
                            }
                            context.stroke()
                            context.beginPath()
                            context.moveTo(-4, -3)
                            context.lineTo(4, -3)
                            context.stroke()
                            context.restore()

                            // The small drop makes the bucket silhouette clear
                            // even when its selected color matches the toolbar.
                            context.beginPath()
                            context.moveTo(12.5, 9)
                            context.bezierCurveTo(11.5, 10.5, 11, 11.2, 11, 12)
                            context.bezierCurveTo(11, 13.1, 11.8, 14, 12.8, 14)
                            context.bezierCurveTo(13.8, 14, 14.5, 13.1, 14.5, 12)
                            context.bezierCurveTo(14.5, 11.2, 13.8, 10.2, 12.5, 9)
                            context.closePath()
                            context.fillStyle = filled ? fillColor : iconColor
                            context.fill()
                            context.stroke()
                        }
                    }
                }
                ToolSeparator { width: 5; height: 26 }
                ToolButton {
                    id: drawingUndoButton
                    objectName: root.objectName + "DrawingUndoButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    enabled: imageInput.hasImage && imageInput.canUndoDrawing
                    Accessible.name: qsTr("Undo drawing change")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name + qsTr(" (Ctrl+Z)")
                    onClicked: imageInput.undoDrawing()

                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: drawingUndoButton.enabled
                                                  ? drawingUndoButton.palette.buttonText
                                                  : drawingUndoButton.palette.mid
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.lineWidth = 1.5
                            context.lineCap = "round"
                            context.lineJoin = "round"
                            context.beginPath()
                            context.moveTo(6, 4)
                            context.lineTo(2, 8)
                            context.lineTo(6, 12)
                            context.moveTo(2, 8)
                            context.bezierCurveTo(8, 4, 14, 6, 14, 12)
                            context.stroke()
                        }
                    }
                }
                ToolButton {
                    id: drawingRedoButton
                    objectName: root.objectName + "DrawingRedoButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    enabled: imageInput.hasImage && imageInput.canRedoDrawing
                    Accessible.name: qsTr("Redo drawing change")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name + qsTr(" (Ctrl+Y)")
                    onClicked: imageInput.redoDrawing()

                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: drawingRedoButton.enabled
                                                  ? drawingRedoButton.palette.buttonText
                                                  : drawingRedoButton.palette.mid
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.lineWidth = 1.5
                            context.lineCap = "round"
                            context.lineJoin = "round"
                            context.beginPath()
                            context.moveTo(10, 4)
                            context.lineTo(14, 8)
                            context.lineTo(10, 12)
                            context.moveTo(14, 8)
                            context.bezierCurveTo(8, 4, 2, 6, 2, 12)
                            context.stroke()
                        }
                    }
                }
                ToolSeparator { width: 5; height: 26 }
                Item {
                width: 18
                height: 26
                Rectangle {
                    anchors.centerIn: parent
                    width: 7
                    height: 7
                    radius: width / 2
                    color: pencilButton.palette.buttonText
                }
                }
                SpinBox {
                    id: drawingDiameterField
                    objectName: root.objectName + "DrawingDiameterField"
                    implicitWidth: 44
                    implicitHeight: 26
                    from: 1
                    to: 64
                    editable: true
                    value: root.drawingDiameter
                    enabled: imageInput.hasImage
                    leftPadding: 3
                    rightPadding: 15
                    Accessible.name: qsTr("Pencil and eraser diameter")
                    onValueModified: root.drawingDiameter = value

                    up.indicator: Rectangle {
                        objectName: root.objectName + "DrawingDiameterUpIndicator"
                        x: drawingDiameterField.width - width
                        y: 0
                        implicitWidth: 14
                        implicitHeight: drawingDiameterField.height / 2
                        color: drawingDiameterField.up.pressed
                               ? drawingDiameterField.palette.mid
                               : drawingDiameterField.palette.button
                        border.color: drawingDiameterField.palette.mid

                        Canvas {
                            anchors.centerIn: parent
                            width: 7
                            height: 4
                            property color arrowColor: drawingDiameterField.enabled
                                                       ? drawingDiameterField.palette.buttonText
                                                       : drawingDiameterField.palette.mid
                            onArrowColorChanged: requestPaint()
                            onPaint: {
                                const context = getContext("2d")
                                context.clearRect(0, 0, width, height)
                                context.fillStyle = arrowColor
                                context.beginPath()
                                context.moveTo(0, height)
                                context.lineTo(width / 2, 0)
                                context.lineTo(width, height)
                                context.closePath()
                                context.fill()
                            }
                        }
                    }

                    down.indicator: Rectangle {
                        objectName: root.objectName + "DrawingDiameterDownIndicator"
                        x: drawingDiameterField.width - width
                        y: drawingDiameterField.height - height
                        implicitWidth: 14
                        implicitHeight: drawingDiameterField.height / 2
                        color: drawingDiameterField.down.pressed
                               ? drawingDiameterField.palette.mid
                               : drawingDiameterField.palette.button
                        border.color: drawingDiameterField.palette.mid

                        Canvas {
                            anchors.centerIn: parent
                            width: 7
                            height: 4
                            property color arrowColor: drawingDiameterField.enabled
                                                       ? drawingDiameterField.palette.buttonText
                                                       : drawingDiameterField.palette.mid
                            onArrowColorChanged: requestPaint()
                            onPaint: {
                                const context = getContext("2d")
                                context.clearRect(0, 0, width, height)
                                context.fillStyle = arrowColor
                                context.beginPath()
                                context.moveTo(0, 0)
                                context.lineTo(width / 2, height)
                                context.lineTo(width, 0)
                                context.closePath()
                                context.fill()
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: viewport
            objectName: root.objectName + "Viewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 180
            color: "#101317"
            border.color: "#53606d"
            radius: 4
            clip: true

            Flickable {
                id: flick
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: Math.max(width, preview.width)
                contentHeight: Math.max(height, preview.height)

                Image {
                    id: preview
                    objectName: root.objectName + "Image"
                    source: root.imageSource
                    asynchronous: !root.sourceTools
                    retainWhileLoading: true
                    cache: false
                    width: Math.max(1, sourceSize.width * root.effectiveZoom)
                    height: Math.max(1, sourceSize.height * root.effectiveZoom)
                    x: Math.max(0, (flick.width - width) / 2)
                    y: Math.max(0, (flick.height - height) / 2)
                    fillMode: Image.Stretch
                    smooth: root.effectiveZoom < 1

                    Canvas {
                        id: strokeOverlay
                        objectName: root.objectName + "StrokeOverlay"
                        anchors.fill: parent
                        z: 1
                        visible: root.sourceTools
                        antialiasing: !root.hardDrawingEdges
                        property var segments: []
                        property point lastPoint: Qt.point(0, 0)
                        property bool shapeActive: false
                        property bool shapeEllipse: false
                        property real shapeStartX: 0
                        property real shapeStartY: 0
                        property real shapeEndX: 0
                        property real shapeEndY: 0
                        property bool hardEdges: root.hardDrawingEdges
                        property bool shapeFilled: root.fillDrawingShapes
                        onHardEdgesChanged: requestPaint()
                        onShapeFilledChanged: requestPaint()

                        function strokeColor() {
                            return String(root.drawingTool === 2
                                          ? imageInput.backgroundColor
                                          : imageInput.foregroundColor)
                        }

                        function beginStroke(x, y) {
                            clearStrokeTimer.stop()
                            segments = []
                            shapeActive = false
                            lastPoint = Qt.point(x, y)
                            segments.push({x1: x, y1: y, x2: x, y2: y,
                                           color: strokeColor(),
                                           diameter: Math.max(1,
                                               root.drawingDiameter * root.effectiveZoom)})
                            requestPaint()
                        }

                        function constrainedShapeEnd(x, y, locked) {
                            if (!locked)
                                return Qt.point(x, y)
                            const deltaX = x - shapeStartX
                            const deltaY = y - shapeStartY
                            const size = Math.max(Math.abs(deltaX), Math.abs(deltaY))
                            return Qt.point(shapeStartX + (deltaX < 0 ? -size : size),
                                            shapeStartY + (deltaY < 0 ? -size : size))
                        }

                        function beginShape(x, y, ellipse) {
                            clearStrokeTimer.stop()
                            segments = []
                            shapeActive = true
                            shapeEllipse = ellipse
                            shapeStartX = x
                            shapeStartY = y
                            shapeEndX = x
                            shapeEndY = y
                            requestPaint()
                        }

                        function updateShape(x, y, locked) {
                            const point = constrainedShapeEnd(x, y, locked)
                            shapeEndX = point.x
                            shapeEndY = point.y
                            requestPaint()
                            return point
                        }

                        function finishShape() {
                            clearStrokeTimer.restart()
                        }

                        function cancelShape() {
                            shapeActive = false
                            requestPaint()
                        }

                        function extendStroke(x, y) {
                            segments.push({x1: lastPoint.x, y1: lastPoint.y,
                                           x2: x, y2: y,
                                           color: strokeColor(),
                                           diameter: Math.max(1,
                                               root.drawingDiameter * root.effectiveZoom)})
                            lastPoint = Qt.point(x, y)
                            requestPaint()
                        }

                        function finishStroke() {
                            clearStrokeTimer.restart()
                        }

                        function clearStroke() {
                            segments = []
                            shapeActive = false
                            requestPaint()
                        }

                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.globalAlpha = 1.0
                            context.imageSmoothingEnabled = !hardEdges
                            context.lineCap = "round"
                            context.lineJoin = "round"
                            for (let index = 0; index < segments.length; ++index) {
                                const segment = segments[index]
                                context.strokeStyle = segment.color
                                context.fillStyle = segment.color
                                context.lineWidth = segment.diameter
                                if (segment.x1 === segment.x2 && segment.y1 === segment.y2) {
                                    context.beginPath()
                                    context.arc(segment.x1, segment.y1,
                                                segment.diameter / 2, 0, Math.PI * 2)
                                    context.fill()
                                } else {
                                    context.beginPath()
                                    context.moveTo(segment.x1, segment.y1)
                                    context.lineTo(segment.x2, segment.y2)
                                    context.stroke()
                                }
                            }
                            if (shapeActive) {
                                const left = Math.min(shapeStartX, shapeEndX)
                                const top = Math.min(shapeStartY, shapeEndY)
                                const shapeWidth = Math.abs(shapeEndX - shapeStartX)
                                const shapeHeight = Math.abs(shapeEndY - shapeStartY)
                                context.strokeStyle = String(imageInput.foregroundColor)
                                context.fillStyle = String(imageInput.foregroundColor)
                                context.lineWidth = Math.max(
                                    1, root.drawingDiameter * root.effectiveZoom)
                                context.beginPath()
                                if (shapeWidth < 0.5 && shapeHeight < 0.5) {
                                    context.arc(shapeStartX, shapeStartY,
                                                context.lineWidth / 2,
                                                0, Math.PI * 2)
                                    context.fill()
                                } else if (shapeEllipse) {
                                    const centerX = left + shapeWidth / 2
                                    const centerY = top + shapeHeight / 2
                                    const radiusX = shapeWidth / 2
                                    const radiusY = shapeHeight / 2
                                    const steps = Math.max(24, Math.min(192,
                                        Math.ceil(Math.max(shapeWidth, shapeHeight) * 0.8)))
                                    context.moveTo(centerX + radiusX, centerY)
                                    for (let step = 1; step <= steps; ++step) {
                                        const angle = Math.PI * 2 * step / steps
                                        context.lineTo(centerX + Math.cos(angle) * radiusX,
                                                       centerY + Math.sin(angle) * radiusY)
                                    }
                                    context.stroke()
                                } else {
                                    context.strokeRect(left, top, shapeWidth, shapeHeight)
                                }
                                if (shapeFilled && shapeWidth >= 0.5 && shapeHeight >= 0.5) {
                                    const inset = Math.max(
                                        0, context.lineWidth / 2 - (hardEdges ? 0 : 1))
                                    const innerWidth = shapeWidth - inset * 2
                                    const innerHeight = shapeHeight - inset * 2
                                    if (innerWidth > 0 && innerHeight > 0) {
                                        context.fillStyle = String(imageInput.backgroundColor)
                                        if (shapeEllipse) {
                                            const innerCenterX = left + shapeWidth / 2
                                            const innerCenterY = top + shapeHeight / 2
                                            const innerRadiusX = innerWidth / 2
                                            const innerRadiusY = innerHeight / 2
                                            const fillSteps = Math.max(24, Math.min(192,
                                                Math.ceil(Math.max(innerWidth, innerHeight) * 0.8)))
                                            context.beginPath()
                                            context.moveTo(innerCenterX + innerRadiusX,
                                                           innerCenterY)
                                            for (let step = 1; step <= fillSteps; ++step) {
                                                const angle = Math.PI * 2 * step / fillSteps
                                                context.lineTo(
                                                    innerCenterX + Math.cos(angle) * innerRadiusX,
                                                    innerCenterY + Math.sin(angle) * innerRadiusY)
                                            }
                                            context.closePath()
                                            context.fill()
                                        } else {
                                            context.fillRect(left + inset, top + inset,
                                                             innerWidth, innerHeight)
                                        }
                                    }
                                }
                            }
                        }

                        Timer {
                            id: clearStrokeTimer
                            interval: 40
                            repeat: false
                            onTriggered: strokeOverlay.clearStroke()
                        }
                    }

                    MouseArea {
                        id: drawingMouseArea
                        z: 2
                        anchors.fill: parent
                        enabled: root.sourceTools && imageInput.hasImage
                                 && root.drawingTool !== 0
                        acceptedButtons: Qt.LeftButton
                        hoverEnabled: true
                        preventStealing: true
                        cursorShape: Qt.CrossCursor
                        property int dragTool: 0
                        onPressed: mouse => {
                            dragTool = root.drawingTool
                            if (dragTool <= 2) {
                                strokeOverlay.beginStroke(mouse.x, mouse.y)
                                imageInput.beginSourceStroke(
                                    mouse.x / Math.max(1, width),
                                    mouse.y / Math.max(1, height),
                                    root.drawingDiameter,
                                    dragTool === 2,
                                    root.hardDrawingEdges)
                            } else {
                                strokeOverlay.beginShape(mouse.x, mouse.y,
                                                         dragTool === 3)
                            }
                        }
                        onPositionChanged: mouse => {
                            if (pressed) {
                                if (dragTool <= 2) {
                                    strokeOverlay.extendStroke(mouse.x, mouse.y)
                                    imageInput.continueSourceStroke(
                                        mouse.x / Math.max(1, width),
                                        mouse.y / Math.max(1, height))
                                } else {
                                    strokeOverlay.updateShape(
                                        mouse.x, mouse.y,
                                        (mouse.modifiers & Qt.ShiftModifier) !== 0)
                                }
                            }
                        }
                        onReleased: mouse => {
                            if (dragTool <= 2) {
                                imageInput.endSourceStroke()
                                strokeOverlay.finishStroke()
                            } else {
                                strokeOverlay.updateShape(
                                    mouse.x, mouse.y,
                                    (mouse.modifiers & Qt.ShiftModifier) !== 0)
                                imageInput.drawSourceShape(
                                    strokeOverlay.shapeStartX / Math.max(1, width),
                                    strokeOverlay.shapeStartY / Math.max(1, height),
                                    strokeOverlay.shapeEndX / Math.max(1, width),
                                    strokeOverlay.shapeEndY / Math.max(1, height),
                                    root.drawingDiameter,
                                    dragTool === 3,
                                    root.hardDrawingEdges,
                                    root.fillDrawingShapes)
                                strokeOverlay.finishShape()
                            }
                            dragTool = 0
                        }
                        onCanceled: {
                            if (dragTool <= 2) {
                                imageInput.endSourceStroke()
                                strokeOverlay.finishStroke()
                            } else {
                                strokeOverlay.cancelShape()
                            }
                            dragTool = 0
                        }
                    }

                    Item {
                        id: brushCursorRing
                        objectName: root.objectName + "BrushCursorRing"
                        z: 3
                        readonly property real cursorDiameter: Math.max(
                            1, root.drawingDiameter * root.effectiveZoom)
                        width: Math.max(3, cursorDiameter)
                        height: width
                        x: drawingMouseArea.mouseX - width / 2
                        y: drawingMouseArea.mouseY - height / 2
                        visible: drawingMouseArea.enabled
                                 && drawingMouseArea.containsMouse

                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            color: "transparent"
                            border.width: 3
                            border.color: "#202020"
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: "#f5f5f5"
                        }
                    }

                    TapHandler {
                        enabled: root.sourceTools && root.pickingColor
                        acceptedButtons: Qt.LeftButton
                        onTapped: (eventPoint, button) => {
                            root.colorPointPicked(
                                eventPoint.position.x / Math.max(1, preview.width),
                                eventPoint.position.y / Math.max(1, preview.height),
                                root.activeColor === 0)
                            root.pickingColor = false
                        }
                    }

                    HoverHandler {
                        enabled: root.sourceTools && root.pickingColor
                        cursorShape: Qt.CrossCursor
                    }
                }

                Rectangle {
                    visible: root.cropOverlay && preview.status === Image.Ready
                    x: preview.x + Math.max(8, preview.width * 0.08)
                    y: preview.y + Math.max(8, preview.height * 0.08)
                    width: Math.max(1, Math.min(preview.width - 16,
                                               (preview.height - 16) * 4 / 3))
                    height: width * 3 / 4
                    color: "transparent"
                    border.width: 2
                    border.color: "#ffcc48"
                    opacity: 0.9
                }
            }

            Image {
                objectName: root.objectName + "Watermark"
                anchors.centerIn: parent
                width: Math.max(1, Math.min(parent.width, parent.height) / 2)
                height: width
                source: Qt.resolvedUrl("../assets/icons/NewConvert9918-watermark-512.png")
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                opacity: 0.12
                visible: root.imageSource.toString().length === 0 && !root.busy
                Accessible.name: qsTr("New Convert 9918 logo")
            }

            Label {
                anchors.centerIn: parent
                visible: root.imageSource.toString().length === 0 && !root.busy
                         && root.workspaceContent === null
                         && root.workspaceContent === null
                width: Math.min(parent.width - 32, 360)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: root.emptyText
                color: "#c2c8cf"
            }

            Loader {
                objectName: root.objectName + "WorkspaceContent"
                anchors.fill: parent
                anchors.margins: 6
                z: 5
                sourceComponent: root.workspaceContent
                visible: sourceComponent !== null
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: root.busy
                visible: running
                Accessible.name: qsTr("Conversion in progress")
            }

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: event => root.zoomBy(event.angleDelta.y > 0 ? 1.15 : 0.87)
            }

            DropArea {
                anchors.fill: parent
                enabled: root.acceptDrops
                onDropped: drop => {
                    if (drop.urls.length > 0) {
                        root.fileDropped(drop.urls[0])
                        drop.acceptProposedAction()
                    }
                }
            }
        }

        Label {
            objectName: root.objectName + "Details"
            Layout.fillWidth: true
            text: root.details
            wrapMode: Text.WordWrap
            color: palette.placeholderText
        }

        RowLayout {
            objectName: root.objectName + "Controls"
            Layout.fillWidth: true
            spacing: 1

            Loader {
                objectName: root.objectName + "CustomLowerToolbar"
                sourceComponent: root.lowerToolbarContent
                visible: sourceComponent !== null
                Layout.fillWidth: visible
                Layout.minimumWidth: 0
            }

            Flow {
                objectName: root.objectName + "SourceTools"
                visible: root.sourceTools
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: childrenRect.height
                spacing: 1

                Item {
                    id: colorControl
                    objectName: root.objectName + "BackgroundColorButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    enabled: imageInput.hasImage
                    opacity: enabled ? 1.0 : 0.45
                    Accessible.name: qsTr("Choose foreground or background color")

                    HoverHandler { id: colorControlHover }
                    ToolTip.visible: colorControlHover.hovered
                    ToolTip.text: Accessible.name

                    Rectangle {
                        id: backgroundColorSquare
                        x: 8
                        y: 8
                        width: 16
                        height: 16
                        z: root.activeColor === 1 ? 2 : 1
                        color: imageInput.backgroundColor
                        radius: 3
                        border.width: root.activeColor === 1 ? 2 : 1
                        border.color: root.activeColor === 1
                                       ? palette.highlight : "#707780"
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: {
                                root.activeColor = 1
                                backgroundPalettePopup.open()
                            }
                        }
                    }
                    Rectangle {
                        id: foregroundColorSquare
                        x: 0
                        y: 0
                        width: 16
                        height: 16
                        z: root.activeColor === 0 ? 2 : 1
                        color: imageInput.foregroundColor
                        radius: 3
                        border.width: root.activeColor === 0 ? 2 : 1
                        border.color: root.activeColor === 0
                                       ? palette.highlight : "#707780"
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: {
                                root.activeColor = 0
                                backgroundPalettePopup.open()
                            }
                        }
                    }
                }
                ToolButton {
                    objectName: root.objectName + "EyedropperButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("⌖")
                    checkable: true
                    checked: root.pickingColor
                    enabled: imageInput.hasImage
                    Accessible.name: qsTr("Pick the active color from the source image")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.pickingColor = checked
                        if (checked)
                            root.drawingTool = 0
                    }
                }
                ToolSeparator { width: 5; height: 26 }
                ToolButton {
                    objectName: root.objectName + "MoveLeftButton"
                    implicitWidth: 24
                    implicitHeight: 26
                    text: qsTr("←")
                    enabled: imageInput.hasImage
                    Accessible.name: qsTr("Move source left")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: imageInput.nudgeSource(-1, 0)
                }
                ToolButton {
                    objectName: root.objectName + "MoveUpButton"
                    implicitWidth: 24
                    implicitHeight: 26
                    text: qsTr("↑")
                    enabled: imageInput.hasImage
                    Accessible.name: qsTr("Move source up")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: imageInput.nudgeSource(0, -1)
                }
                ToolButton {
                    objectName: root.objectName + "CenterButton"
                    implicitWidth: 24
                    implicitHeight: 26
                    text: qsTr("◎")
                    enabled: imageInput.hasImage
                             && (imageInput.horizontalOffset !== 0
                                 || imageInput.verticalOffset !== 0)
                    Accessible.name: qsTr("Center source")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: imageInput.centerSource()
                }
                ToolButton {
                    objectName: root.objectName + "MoveDownButton"
                    implicitWidth: 24
                    implicitHeight: 26
                    text: qsTr("↓")
                    enabled: imageInput.hasImage
                    Accessible.name: qsTr("Move source down")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: imageInput.nudgeSource(0, 1)
                }
                ToolButton {
                    objectName: root.objectName + "MoveRightButton"
                    implicitWidth: 24
                    implicitHeight: 26
                    text: qsTr("→")
                    enabled: imageInput.hasImage
                    Accessible.name: qsTr("Move source right")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: imageInput.nudgeSource(1, 0)
                }
            }

            Flow {
                objectName: root.objectName + "ConvertedTools"
                visible: root.convertedTools
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: childrenRect.height
                spacing: 1

                Switch {
                    objectName: root.convertedTools
                                ? root.objectName + "AutoUpdateSwitch" : ""
                    implicitHeight: 26
                    leftPadding: 1
                    rightPadding: 1
                    spacing: 2
                    text: qsTr("Auto")
                    checked: imageInput.autoUpdate
                    Accessible.name: qsTr("Automatically update conversion")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onToggled: imageInput.autoUpdate = checked
                }
            }

            Item {
                visible: !root.sourceTools && !root.convertedTools
                         && root.lowerToolbarContent === null
                Layout.fillWidth: true
            }

            RowLayout {
                objectName: root.objectName + "ZoomControls"
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                spacing: 1

                ToolButton {
                    id: zoomMenuButton
                    objectName: root.objectName + "ZoomMenuButton"
                    readonly property real renderedContentWidth: contentItem.implicitWidth
                    implicitWidth: Math.ceil(contentItem.implicitWidth)
                                   + leftPadding + rightPadding
                    implicitHeight: 26
                    leftPadding: 2
                    rightPadding: 2
                    text: qsTr("🔍 %1%").arg(Math.round(root.effectiveZoom * 100))
                    contentItem: Text {
                        text: zoomMenuButton.text
                        font: zoomMenuButton.font
                        color: zoomMenuButton.palette.buttonText
                        opacity: zoomMenuButton.enabled ? 1.0 : 0.45
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    enabled: root.imageSource.toString().length > 0
                    Accessible.name: qsTr("Zoom options for %1; current zoom %2 percent")
                                     .arg(root.title)
                                     .arg(Math.round(root.effectiveZoom * 100))
                    activeFocusOnTab: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Zoom options")
                    onClicked: zoomMenu.open()

                    Menu {
                        id: zoomMenu
                        objectName: root.objectName + "ZoomMenu"
                        y: parent.height

                        MenuItem {
                            objectName: root.objectName + "FitMenuItem"
                            text: qsTr("Fit to view")
                            onTriggered: root.fitToView = true
                        }
                        MenuItem {
                            objectName: root.objectName + "ActualSizeMenuItem"
                            text: qsTr("Actual size (1:1)")
                            onTriggered: {
                                root.fitToView = false
                                root.manualZoom = 1.0
                            }
                        }
                    }
                }

                ColumnLayout {
                    id: zoomStepButtons
                    objectName: root.objectName + "ZoomStepButtons"
                    readonly property real compactWidth:
                        Math.ceil(Math.max(zoomInLabel.implicitWidth,
                                           zoomOutLabel.implicitWidth)) + 4
                    spacing: 0
                    Layout.minimumWidth: compactWidth
                    Layout.preferredWidth: compactWidth
                    Layout.maximumWidth: compactWidth
                    implicitHeight: 26

                    ToolButton {
                        id: zoomInButton
                        objectName: root.objectName + "ZoomInButton"
                        readonly property real renderedContentWidth:
                            contentItem.implicitWidth
                        Layout.fillWidth: true
                        Layout.preferredHeight: 13
                        implicitWidth: zoomStepButtons.compactWidth
                        leftPadding: 2
                        rightPadding: 2
                        topPadding: 0
                        bottomPadding: 0
                        text: qsTr("+")
                        font.pixelSize: 11
                        contentItem: Text {
                            id: zoomInLabel
                            text: zoomInButton.text
                            font: zoomInButton.font
                            color: zoomInButton.palette.buttonText
                            opacity: zoomInButton.enabled ? 1.0 : 0.45
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        enabled: root.imageSource.toString().length > 0
                        Accessible.name: qsTr("Zoom in %1").arg(root.title)
                        activeFocusOnTab: true
                        onClicked: root.zoomBy(1.25)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Zoom in")
                    }
                    ToolButton {
                        id: zoomOutButton
                        objectName: root.objectName + "ZoomOutButton"
                        readonly property real renderedContentWidth:
                            contentItem.implicitWidth
                        Layout.fillWidth: true
                        Layout.preferredHeight: 13
                        implicitWidth: zoomStepButtons.compactWidth
                        leftPadding: 2
                        rightPadding: 2
                        topPadding: 0
                        bottomPadding: 0
                        text: qsTr("−")
                        font.pixelSize: 11
                        contentItem: Text {
                            id: zoomOutLabel
                            text: zoomOutButton.text
                            font: zoomOutButton.font
                            color: zoomOutButton.palette.buttonText
                            opacity: zoomOutButton.enabled ? 1.0 : 0.45
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        enabled: root.imageSource.toString().length > 0
                        Accessible.name: qsTr("Zoom out %1").arg(root.title)
                        activeFocusOnTab: true
                        onClicked: root.zoomBy(0.8)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Zoom out")
                    }
                }
            }
        }
    }
}

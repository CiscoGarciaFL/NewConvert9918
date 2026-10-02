import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ThemedFrame {
    id: root

    required property string title
    property url imageSource
    property string emptyText: qsTr("No image")
    property string details
    property string detailsTrailingText
    property bool busy: false
    property bool cropOverlay: false
    property bool acceptDrops: false
    property bool showTitle: true
    property bool sourceTools: false
    property bool screenImageTools: false
    property bool convertedTools: false
    property int activeColor: 1 // 0 = foreground, 1 = background
    property bool pickingColor: false
    // 0 = none, 1 = pencil, 2 = eraser, 3 = ellipse, 4 = rectangle,
    // 5 = line, 6 = chained K-Line, 7 = fixed-origin Rays, 8 = color swap,
    // 9 = rectangular selection, 10 = floating selection placement.
    property int drawingTool: 0
    property int drawingDiameter: 1
    property bool hardDrawingEdges: false
    property bool squareDrawingBrush: false
    property bool fillDrawingShapes: false
    property bool gridVisible: false
    property bool snapDrawingToCharacterBounds: false
    property Component upperToolbarContent
    property Component workspaceContent
    property Component lowerToolbarContent
    property bool zoomInteractive: true
    property bool zoomContentAvailable: imageSource.toString().length > 0
    property real pixelAspectRatio: 1.0
    readonly property bool editingTools: sourceTools || screenImageTools
    readonly property bool editableContentAvailable: screenImageTools
                                                     ? imageInput.hasConversion
                                                       && !imageInput.busy
                                                     : imageInput.hasImage
    readonly property color foregroundPaintColor: screenImageTools
                                                  ? imageInput.screenImageForegroundColor
                                                  : imageInput.foregroundColor
    readonly property color backgroundPaintColor: screenImageTools
                                                  ? imageInput.screenImageBackgroundColor
                                                  : imageInput.backgroundColor
    readonly property bool hasSelection: screenImageTools
                                         ? imageInput.hasScreenImageSelection
                                         : imageInput.hasSourceSelection
    readonly property int selectionX: screenImageTools
                                      ? imageInput.screenImageSelectionX
                                      : imageInput.sourceSelectionX
    readonly property int selectionY: screenImageTools
                                      ? imageInput.screenImageSelectionY
                                      : imageInput.sourceSelectionY
    readonly property int selectionWidth: screenImageTools
                                          ? imageInput.screenImageSelectionWidth
                                          : imageInput.sourceSelectionWidth
    readonly property int selectionHeight: screenImageTools
                                           ? imageInput.screenImageSelectionHeight
                                           : imageInput.sourceSelectionHeight
    readonly property bool floating: screenImageTools
                                     ? imageInput.screenImageFloating
                                     : imageInput.sourceFloating
    readonly property bool floatingMove: screenImageTools
                                         ? imageInput.screenImageFloatingMove
                                         : imageInput.sourceFloatingMove
    readonly property int floatingWidth: screenImageTools
                                         ? imageInput.screenImageFloatingWidth
                                         : imageInput.sourceFloatingWidth
    readonly property int floatingHeight: screenImageTools
                                          ? imageInput.screenImageFloatingHeight
                                          : imageInput.sourceFloatingHeight
    readonly property string floatingPreview: screenImageTools
                                              ? imageInput.screenImageFloatingPreview
                                              : imageInput.sourceFloatingPreview
    readonly property bool floatingTopLeft: screenImageTools
                                           ? imageInput.screenImageFloatingTopLeft
                                           : imageInput.sourceFloatingTopLeft
    readonly property var availableFonts: screenImageTools
                                          ? imageInput.screenImageFonts
                                          : imageInput.sourceImageFonts
    signal fileDropped(url fileUrl)
    signal colorPointPicked(real normalizedX, real normalizedY, bool foreground)
    signal editingSurfaceActivated(bool screenImage)

    property bool fitToView: true
    property real manualZoom: 1.0
    readonly property real fitZoom: {
        if (preview.sourceSize.width <= 0 || preview.sourceSize.height <= 0)
            return 1.0
        return Math.min(viewport.width
                            / (preview.sourceSize.width * root.pixelAspectRatio),
                        viewport.height / preview.sourceSize.height)
    }
    readonly property real effectiveZoom: zoomInteractive && zoomContentAvailable
                                          ? (fitToView ? fitZoom : manualZoom)
                                          : 1.0

    function zoomBy(factor) {
        if (!zoomInteractive || !zoomContentAvailable)
            return
        const startingZoom = effectiveZoom
        manualZoom = Math.max(0.125, Math.min(16, startingZoom * factor))
        fitToView = false
    }

    function chooseActiveColor(color) {
        root.editingSurfaceActivated(root.screenImageTools)
        if (root.screenImageTools) {
            if (root.activeColor === 0)
                imageInput.screenImageForegroundColor = color
            else
                imageInput.screenImageBackgroundColor = color
        } else if (root.activeColor === 0) {
            imageInput.foregroundColor = color
        } else {
            imageInput.backgroundColor = color
        }
        backgroundPalettePopup.close()
    }

    function finishMultiLine() {
        if (!strokeOverlay.polylineActive)
            return
        if (root.screenImageTools)
            imageInput.endScreenImageStroke()
        else
            imageInput.endSourceStroke()
        strokeOverlay.finishPolyline()
    }

    function finishKLine() {
        if (root.editingTools)
            root.editingSurfaceActivated(root.screenImageTools)
        finishMultiLine()
    }

    function selectDrawingTool(tool) {
        root.editingSurfaceActivated(root.screenImageTools)
        if ((root.drawingTool === 6 || root.drawingTool === 7)
                && tool !== root.drawingTool)
            root.finishMultiLine()
        if (root.floating && tool !== 10) {
            if (root.screenImageTools) imageInput.cancelScreenImageFloating()
            else imageInput.cancelSourceFloating()
        }
        root.drawingTool = tool
        root.pickingColor = false
    }

    function positionFloatingAtSelectionOrCenter() {
        const imageWidth = Math.max(1, preview.sourceSize.width)
        const imageHeight = Math.max(1, preview.sourceSize.height)
        if (root.hasSelection) {
            drawingMouseArea.floatingCenterX =
                (root.selectionX
                 + (root.floatingTopLeft ? 0 : root.selectionWidth / 2))
                * preview.width / imageWidth
            drawingMouseArea.floatingCenterY =
                (root.selectionY
                 + (root.floatingTopLeft ? 0 : root.selectionHeight / 2))
                * preview.height / imageHeight
        } else {
            drawingMouseArea.floatingCenterX = preview.width / 2
            drawingMouseArea.floatingCenterY = preview.height / 2
        }
        drawingMouseArea.forceActiveFocus()
    }

    function drawingDiameterForTool(tool) {
        if (!root.screenImageTools || !root.snapDrawingToCharacterBounds
                || tool < 3)
            return root.drawingDiameter
        // A brush wider than the target character cell cannot remain inside it.
        const cellSize = Math.min(editorProject.characterPatternWidth,
                                  editorProject.characterPatternHeight)
        return Math.min(root.drawingDiameter,
                        root.hardDrawingEdges ? cellSize
                                              : Math.max(1, cellSize - 1))
    }

    function characterSnapInset(tool) {
        const diameter = drawingDiameterForTool(tool)
        return diameter / 2 + (root.hardDrawingEdges ? 0 : 0.5)
    }

    function characterCellTopLeft(localX, localY, tool) {
        const imageWidth = Math.max(1, preview.sourceSize.width)
        const imageHeight = Math.max(1, preview.sourceSize.height)
        const sourceX = Math.max(0, Math.min(imageWidth - 0.001,
                                             localX * imageWidth
                                             / Math.max(1, preview.width)))
        const sourceY = Math.max(0, Math.min(imageHeight - 0.001,
                                             localY * imageHeight
                                             / Math.max(1, preview.height)))
        const inset = characterSnapInset(tool)
        const cellWidth = editorProject.characterPatternWidth
        const cellHeight = editorProject.characterPatternHeight
        return Qt.point((Math.floor(sourceX / cellWidth) * cellWidth + inset)
                            * preview.width / imageWidth,
                        (Math.floor(sourceY / cellHeight) * cellHeight + inset)
                            * preview.height / imageHeight)
    }

    function characterCellBottomRight(localX, localY, tool) {
        const imageWidth = Math.max(1, preview.sourceSize.width)
        const imageHeight = Math.max(1, preview.sourceSize.height)
        const sourceX = Math.max(0, Math.min(imageWidth - 0.001,
                                             localX * imageWidth
                                             / Math.max(1, preview.width)))
        const sourceY = Math.max(0, Math.min(imageHeight - 0.001,
                                             localY * imageHeight
                                             / Math.max(1, preview.height)))
        const inset = characterSnapInset(tool)
        const cellWidth = editorProject.characterPatternWidth
        const cellHeight = editorProject.characterPatternHeight
        return Qt.point(Math.min(imageWidth - inset,
                                 (Math.floor(sourceX / cellWidth) + 1)
                                 * cellWidth - inset)
                            * preview.width / imageWidth,
                        Math.min(imageHeight - inset,
                                 (Math.floor(sourceY / cellHeight) + 1)
                                 * cellHeight - inset)
                            * preview.height / imageHeight)
    }

    function characterSelectionPoint(localX, localY, endPoint) {
        if (!root.screenImageTools || !root.snapDrawingToCharacterBounds)
            return Qt.point(localX, localY)
        const imageWidth = Math.max(1, preview.sourceSize.width)
        const imageHeight = Math.max(1, preview.sourceSize.height)
        const sourceX = Math.max(0, Math.min(imageWidth - 0.001,
                                             localX * imageWidth
                                             / Math.max(1, preview.width)))
        const sourceY = Math.max(0, Math.min(imageHeight - 0.001,
                                             localY * imageHeight
                                             / Math.max(1, preview.height)))
        const cellWidth = editorProject.characterPatternWidth
        const cellHeight = editorProject.characterPatternHeight
        const pixelX = endPoint
            ? Math.min(imageWidth - 1,
                       (Math.floor(sourceX / cellWidth) + 1) * cellWidth - 1)
            : Math.floor(sourceX / cellWidth) * cellWidth
        const pixelY = endPoint
            ? Math.min(imageHeight - 1,
                       (Math.floor(sourceY / cellHeight) + 1) * cellHeight - 1)
            : Math.floor(sourceY / cellHeight) * cellHeight
        return Qt.point(pixelX * preview.width / imageWidth,
                        pixelY * preview.height / imageHeight)
    }

    function drawingStartPoint(localX, localY, tool) {
        return root.screenImageTools && root.snapDrawingToCharacterBounds
               && tool >= 3 ? characterCellTopLeft(localX, localY, tool)
                            : Qt.point(localX, localY)
    }

    function drawingEndPoint(localX, localY, tool) {
        return root.screenImageTools && root.snapDrawingToCharacterBounds
               && tool >= 3 ? characterCellBottomRight(localX, localY, tool)
                            : Qt.point(localX, localY)
    }

    Connections {
        target: imageInput
        function onScreenImageFloatingChanged() {
            if (!root.screenImageTools)
                return
            if (imageInput.screenImageFloating) {
                root.drawingTool = 10
                root.pickingColor = false
                root.positionFloatingAtSelectionOrCenter()
            } else if (root.drawingTool === 10) {
                root.drawingTool = imageInput.hasScreenImageSelection ? 9 : 0
            }
        }
        function onSourceFloatingChanged() {
            if (!root.sourceTools)
                return
            if (imageInput.sourceFloating) {
                root.drawingTool = 10
                root.pickingColor = false
                root.positionFloatingAtSelectionOrCenter()
            } else if (root.drawingTool === 10) {
                root.drawingTool = imageInput.hasSourceSelection ? 9 : 0
            }
        }
    }

    Dialog {
        id: typeToolDialog
        objectName: root.objectName + "TypeToolDialog"
        title: qsTr("Type")
        modal: true
        width: Math.max(360, Math.min(620, root.width - 32))
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            if (fontCombo.currentIndex < 0)
                return
            if (root.screenImageTools) {
                imageInput.prepareScreenImageText(
                    typeText.text, root.availableFonts[fontCombo.currentIndex].key,
                    typeSize.value)
            } else {
                imageInput.prepareSourceText(
                    typeText.text, root.availableFonts[fontCombo.currentIndex].key,
                    typeSize.value)
            }
        }

        ColumnLayout {
            width: parent.width
            spacing: 8

            Label { text: qsTr("Text") }
            TextArea {
                id: typeText
                objectName: root.objectName + "TypeText"
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                placeholderText: qsTr("Enter text to place")
                wrapMode: TextEdit.NoWrap
                selectByMouse: true
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Font") }
                ComboBox {
                    id: fontCombo
                    objectName: root.objectName + "TypeFontComboBox"
                    Layout.fillWidth: true
                    model: root.availableFonts
                    textRole: "name"
                    valueRole: "key"
                    popup.height: Math.min(420, popup.contentItem.implicitHeight)
                    delegate: ItemDelegate {
                        required property var modelData
                        width: fontCombo.popup.width
                        height: 38
                        contentItem: RowLayout {
                            spacing: 7
                            Rectangle {
                                implicitWidth: 34
                                implicitHeight: 22
                                radius: 3
                                color: palette.highlight
                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.badge
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: palette.highlightedText
                                }
                            }
                            Image {
                                visible: modelData.kind === "tiartist"
                                Layout.preferredWidth: visible ? 150 : 0
                                Layout.preferredHeight: 24
                                source: modelData.preview
                                fillMode: Image.PreserveAspectFit
                                horizontalAlignment: Image.AlignLeft
                                smooth: false
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.name
                                elide: Text.ElideRight
                                font.family: modelData.kind === "system"
                                             ? modelData.family : "monospace"
                            }
                        }
                    }
                    Accessible.name: qsTr("Font and font format")
                }
                Label { text: qsTr("Size") }
                SpinBox {
                    id: typeSize
                    objectName: root.objectName + "TypeSizeSpinBox"
                    from: 1
                    to: imageInput.targetHeight
                    value: 16
                    editable: true
                    Accessible.name: qsTr("Font pixel size")
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                color: "#101317"
                border.color: "#53606d"
                radius: 3
                Label {
                    anchors.fill: parent
                    anchors.margins: 8
                    text: typeText.text.length > 0 ? typeText.text : qsTr("Text preview")
                    color: root.foregroundPaintColor
                    font.family: fontCombo.currentIndex >= 0
                                 && root.availableFonts[fontCombo.currentIndex].kind
                                    === "system"
                                 ? root.availableFonts[fontCombo.currentIndex].family
                                 : "monospace"
                    font.pixelSize: Math.min(typeSize.value, 48)
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            RowLayout {
                visible: root.screenImageTools
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: qsTr("TI Artist fonts: %1").arg(imageInput.tiArtistFontsPath)
                    elide: Text.ElideMiddle
                    color: palette.placeholderText
                    ToolTip.visible: fontPathHover.hovered
                    ToolTip.text: imageInput.tiArtistFontsPath
                    HoverHandler { id: fontPathHover }
                }
                Button {
                    text: qsTr("Open Folder")
                    onClicked: imageInput.openTiArtistFontsFolder()
                }
                Button {
                    text: qsTr("Refresh")
                    onClicked: imageInput.reloadScreenImageFonts()
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.snapDrawingToCharacterBounds
                      ? qsTr("Placement starts at the upper-left of the 8×8 cell under the pointer.")
                      : qsTr("Placement starts at the exact pixel under the pointer.")
                color: palette.placeholderText
            }
        }
    }

    FileDialog {
        id: clipArtFileDialog
        title: qsTr("Choose Slide/ClipArt")
        nameFilters: root.screenImageTools
                     ? [
                         qsTr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp *.pcx)"),
                         qsTr("TI and retro art (*.tiap *.tiac *.tiam *_P *_C *_M *.sc2 *.pc *.pp *.hgr *.hgrh)"),
                         qsTr("All files (*)")
                       ]
                     : [
                         qsTr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp *.pcx)"),
                         qsTr("All files (*)")
                       ]
        onAccepted: imageInput.loadScreenImageClipArt(selectedFile)
    }

    Dialog {
        id: clipArtDialog
        objectName: root.objectName + "ClipArtDialog"
        title: qsTr("Slide / ClipArt")
        modal: true
        width: Math.max(360, Math.min(620, root.width - 32))
        property bool changingSize: false
        property real sourceAspect: imageInput.screenImageClipArtSourceHeight > 0
                                    ? imageInput.screenImageClipArtSourceWidth
                                      / imageInput.screenImageClipArtSourceHeight : 1
        property string processedPreview: ""

        function resetSourceSize() {
            if (imageInput.screenImageClipArtSourceWidth <= 0)
                return
            changingSize = true
            clipWidth.value = Math.min(imageInput.targetWidth,
                                       imageInput.screenImageClipArtSourceWidth)
            clipHeight.value = Math.min(imageInput.targetHeight,
                                        imageInput.screenImageClipArtSourceHeight)
            changingSize = false
            refreshPreview()
        }
        function refreshPreview() {
            processedPreview = root.screenImageTools
                ? imageInput.screenImageClipArtPreview(
                    clipWidth.value, clipHeight.value, clipColorMode.currentIndex,
                    clipTransparent.checked, clipUseColors.checked)
                : imageInput.sourceImageClipArtPreview(
                    clipWidth.value, clipHeight.value, clipColorMode.currentIndex,
                    clipTransparent.checked, clipUseColors.checked)
        }
        onOpened: refreshPreview()
        onAccepted: {
            if (root.screenImageTools) {
                imageInput.prepareScreenImageClipArt(
                    clipWidth.value, clipHeight.value, clipColorMode.currentIndex,
                    clipTransparent.checked, clipUseColors.checked)
            } else {
                imageInput.prepareSourceImageClipArt(
                    clipWidth.value, clipHeight.value, clipColorMode.currentIndex,
                    clipTransparent.checked, clipUseColors.checked)
            }
        }

        Connections {
            target: imageInput
            function onScreenImageClipArtChanged() {
                clipArtDialog.resetSourceSize()
            }
            function onScreenImageColorsChanged() {
                if (clipArtDialog.opened)
                    clipArtDialog.refreshPreview()
            }
            function onSettingsChanged() {
                if (root.sourceTools && clipArtDialog.opened)
                    clipArtDialog.refreshPreview()
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Place")
                enabled: imageInput.screenImageClipArtSourceWidth > 0
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }

        ColumnLayout {
            width: parent.width
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Button {
                    objectName: root.objectName + "ClipArtChooseFileButton"
                    text: qsTr("Choose File…")
                    onClicked: clipArtFileDialog.open()
                }
                Label {
                    Layout.fillWidth: true
                    text: imageInput.screenImageClipArtSourceName.length > 0
                          ? imageInput.screenImageClipArtSourceName
                          : qsTr("No art selected")
                    elide: Text.ElideMiddle
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 230
                color: "#101317"
                border.color: "#53606d"
                radius: 3
                Image {
                    id: clipArtPreviewImage
                    objectName: root.objectName + "ClipArtProcessedPreview"
                    anchors.fill: parent
                    anchors.margins: 8
                    source: clipArtDialog.processedPreview
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                }
                Label {
                    anchors.centerIn: parent
                    visible: clipArtPreviewImage.source.toString().length === 0
                    text: qsTr("Choose an image to preview")
                    color: palette.placeholderText
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Width") }
                SpinBox {
                    id: clipWidth
                    objectName: root.objectName + "ClipArtWidthSpinBox"
                    from: 1
                    to: imageInput.targetWidth
                    value: 64
                    editable: true
                    onValueModified: {
                        if (clipKeepAspect.checked && !clipArtDialog.changingSize) {
                            clipArtDialog.changingSize = true
                            clipHeight.value = Math.max(
                                1, Math.min(imageInput.targetHeight, Math.round(value
                                    / clipArtDialog.sourceAspect)))
                            clipArtDialog.changingSize = false
                        }
                        clipArtDialog.refreshPreview()
                    }
                }
                Label { text: qsTr("Height") }
                SpinBox {
                    id: clipHeight
                    objectName: root.objectName + "ClipArtHeightSpinBox"
                    from: 1
                    to: imageInput.targetHeight
                    value: 64
                    editable: true
                    onValueModified: {
                        if (clipKeepAspect.checked && !clipArtDialog.changingSize) {
                            clipArtDialog.changingSize = true
                            clipWidth.value = Math.max(
                                1, Math.min(imageInput.targetWidth, Math.round(value
                                    * clipArtDialog.sourceAspect)))
                            clipArtDialog.changingSize = false
                        }
                        clipArtDialog.refreshPreview()
                    }
                }
                CheckBox {
                    id: clipKeepAspect
                    text: qsTr("Keep aspect")
                    checked: true
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Color") }
                ComboBox {
                    id: clipColorMode
                    objectName: root.objectName + "ClipArtColorModeComboBox"
                    Layout.fillWidth: true
                    model: [qsTr("Original color"), qsTr("Monochrome"),
                            qsTr("Black and white")]
                    onCurrentIndexChanged: clipArtDialog.refreshPreview()
                }
            }
            CheckBox {
                id: clipUseColors
                objectName: root.objectName + "ClipArtUseSelectedColorsCheckBox"
                text: qsTr("Map the art to the selected foreground/background colors")
                onToggled: clipArtDialog.refreshPreview()
            }
            CheckBox {
                id: clipTransparent
                objectName: root.objectName + "ClipArtTransparentCheckBox"
                text: qsTr("Make the background transparent")
                checked: true
                onToggled: clipArtDialog.refreshPreview()
            }
        }
    }

    Popup {
        id: backgroundPalettePopup
        objectName: root.objectName + "ColorPickerPopup"
        enabled: root.editableContentAvailable
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
                            border.width: spectrumColorHover.hovered ? 2 : 1
                            border.color: spectrumColorHover.hovered ? palette.highlight : "#707780"
                            Accessible.name: qsTr("Select %1").arg(String(swatchColor))
                            HoverHandler { id: spectrumColorHover }
                            TapHandler {
                                acceptedButtons: Qt.LeftButton
                                onTapped: root.chooseActiveColor(swatchColor)
                            }
                            ToolTip.visible: spectrumColorHover.hovered
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
                        border.width: standardColorHover.hovered ? 2 : 1
                        border.color: standardColorHover.hovered ? palette.highlight : "#707780"
                        Accessible.name: qsTr("Select %1").arg(String(swatchColor))
                        HoverHandler { id: standardColorHover }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: root.chooseActiveColor(swatchColor)
                        }
                        ToolTip.visible: standardColorHover.hovered
                        ToolTip.text: String(swatchColor)
                    }
                }

                ColumnLayout {
                    spacing: 5
                    Label {
                        Layout.fillWidth: true
                        text: root.screenImageTools && !imageInput.hasImage
                              ? qsTr("Colors used by the Screen Image")
                              : qsTr("Colors found in the source image")
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
                            border.width: usedColorHover.hovered ? 2 : 1
                            border.color: usedColorHover.hovered ? palette.highlight : "#707780"
                            Accessible.name: qsTr("Select %1").arg(String(swatchColor))
                            HoverHandler { id: usedColorHover }
                            TapHandler {
                                acceptedButtons: Qt.LeftButton
                                onTapped: root.chooseActiveColor(swatchColor)
                            }
                            ToolTip.visible: usedColorHover.hovered
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
            Layout.preferredHeight: width < 560 ? 53 : 26

            Loader {
                objectName: root.objectName + "CustomUpperToolbar"
                anchors.fill: parent
                sourceComponent: root.upperToolbarContent
                visible: sourceComponent !== null
            }

            Flow {
                objectName: root.objectName + "DrawingTools"
                anchors.fill: parent
                visible: root.editingTools
                spacing: 1

                ToolButton {
                id: pencilButton
                objectName: root.objectName + "PencilButton"
                implicitWidth: 26
                implicitHeight: 26
                checkable: true
                checked: root.drawingTool === 1
                enabled: root.editableContentAvailable
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
                    root.selectDrawingTool(1)
                }
                }
                ToolButton {
                id: eraserButton
                objectName: root.objectName + "EraserButton"
                implicitWidth: 26
                implicitHeight: 26
                checkable: true
                checked: root.drawingTool === 2
                enabled: root.editableContentAvailable
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
                    root.selectDrawingTool(2)
                }
                }
                ToolButton {
                    id: colorSwapButton
                    objectName: root.objectName + "ColorSwapButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    checkable: true
                    checked: root.drawingTool === 8
                    enabled: root.editableContentAvailable
                    Accessible.name: qsTr("Swap the foreground color with a color in the image")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Color Swap — click an image color to exchange all matching pixels with the foreground color")
                    onClicked: root.selectDrawingTool(8)

                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: colorSwapButton.enabled
                                                  ? colorSwapButton.palette.buttonText
                                                  : colorSwapButton.palette.mid
                        property color foregroundSwatch: root.foregroundPaintColor
                        onIconColorChanged: requestPaint()
                        onForegroundSwatchChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.fillStyle = foregroundSwatch
                            context.strokeStyle = iconColor
                            context.lineWidth = 1
                            context.fillRect(1, 2, 5, 5)
                            context.strokeRect(1, 2, 5, 5)
                            context.fillStyle = iconColor
                            context.fillRect(10, 9, 5, 5)
                            context.beginPath()
                            context.moveTo(6, 4)
                            context.lineTo(12, 4)
                            context.lineTo(10, 2)
                            context.moveTo(10, 12)
                            context.lineTo(4, 12)
                            context.lineTo(6, 14)
                            context.stroke()
                        }
                    }
                }
                ToolButton {
                    id: lineButton
                    objectName: root.objectName + "LineButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    checkable: true
                    checked: root.drawingTool === 5
                    enabled: root.editableContentAvailable
                    Accessible.name: qsTr("Draw a line; hold Shift for horizontal or vertical")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: root.selectDrawingTool(5)
                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: lineButton.enabled
                                                  ? lineButton.palette.buttonText
                                                  : lineButton.palette.mid
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.lineWidth = 2
                            context.lineCap = "round"
                            context.beginPath()
                            context.moveTo(2, 13)
                            context.lineTo(14, 3)
                            context.stroke()
                        }
                    }
                }
                ToolButton {
                    id: kLineButton
                    objectName: root.objectName + "KLineButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    checkable: true
                    checked: root.drawingTool === 6
                    enabled: root.editableContentAvailable
                    Accessible.name: qsTr("Draw connected K-Line segments; Escape finishes")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: root.selectDrawingTool(6)
                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: kLineButton.enabled
                                                  ? kLineButton.palette.buttonText
                                                  : kLineButton.palette.mid
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.fillStyle = iconColor
                            context.lineWidth = 1.5
                            context.lineJoin = "round"
                            context.beginPath()
                            context.moveTo(2, 13)
                            context.lineTo(7, 4)
                            context.lineTo(14, 11)
                            context.stroke()
                            context.beginPath()
                            context.arc(2, 13, 1.5, 0, Math.PI * 2)
                            context.arc(7, 4, 1.5, 0, Math.PI * 2)
                            context.arc(14, 11, 1.5, 0, Math.PI * 2)
                            context.fill()
                        }
                    }
                }
                ToolButton {
                    id: raysButton
                    objectName: root.objectName + "RaysButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    checkable: true
                    checked: root.drawingTool === 7
                    enabled: root.editableContentAvailable
                    Accessible.name: qsTr("Draw fixed-origin Rays; Escape finishes")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: root.selectDrawingTool(7)
                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: raysButton.enabled
                                                  ? raysButton.palette.buttonText
                                                  : raysButton.palette.mid
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.fillStyle = iconColor
                            context.lineWidth = 1.5
                            context.lineCap = "round"
                            context.beginPath()
                            context.moveTo(3, 8)
                            context.lineTo(13, 3)
                            context.moveTo(3, 8)
                            context.lineTo(14, 8)
                            context.moveTo(3, 8)
                            context.lineTo(13, 13)
                            context.stroke()
                            context.beginPath()
                            context.arc(3, 8, 2, 0, Math.PI * 2)
                            context.fill()
                        }
                    }
                }
                ToolButton {
                    id: ellipseButton
                    objectName: root.objectName + "EllipseButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    checkable: true
                    checked: root.drawingTool === 3
                    enabled: root.editableContentAvailable
                    Accessible.name: qsTr("Draw an ellipse; hold Shift for a circle")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.selectDrawingTool(3)
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
                    enabled: root.editableContentAvailable
                    Accessible.name: qsTr("Draw a rectangle; hold Shift for a square")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.selectDrawingTool(4)
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
                    id: characterSnapButton
                    objectName: root.objectName + "CharacterSnapButton"
                    visible: root.screenImageTools
                    implicitWidth: visible ? 26 : 0
                    implicitHeight: 26
                    checkable: true
                    checked: root.snapDrawingToCharacterBounds
                    enabled: root.editableContentAvailable
                    Accessible.name: checked
                                     ? qsTr("Disable 8 by 8 character-bound snapping")
                                     : qsTr("Snap shapes and lines to 8 by 8 character bounds")
                    ToolTip.visible: hovered
                    ToolTip.text: checked
                                      ? qsTr("Character-bound snap is on")
                                      : qsTr("Align shape anchors and endpoints to character-cell corners")
                    onClicked: root.snapDrawingToCharacterBounds = checked

                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: characterSnapButton.enabled
                                                  ? characterSnapButton.palette.buttonText
                                                  : characterSnapButton.palette.mid
                        property bool snapState: characterSnapButton.checked
                        onIconColorChanged: requestPaint()
                        onSnapStateChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.fillStyle = iconColor
                            context.lineWidth = snapState ? 2 : 1
                            context.strokeRect(2, 2, 12, 12)
                            context.beginPath()
                            context.moveTo(8, 2)
                            context.lineTo(8, 14)
                            context.moveTo(2, 8)
                            context.lineTo(14, 8)
                            context.stroke()
                            context.fillRect(1, 1, 3, 3)
                            context.fillRect(12, 12, 3, 3)
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
                    enabled: root.editableContentAvailable
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
                    enabled: root.editableContentAvailable
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
                        property color fillColor: root.backgroundPaintColor
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
                    enabled: root.editableContentAvailable
                             && (root.screenImageTools
                                 ? imageInput.canUndoScreenImage
                                 : imageInput.canUndoDrawing)
                    Accessible.name: qsTr("Undo drawing change")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name + qsTr(" (Ctrl+Z)")
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.undoScreenImage()
                        else imageInput.undoDrawing()
                    }

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
                    enabled: root.editableContentAvailable
                             && (root.screenImageTools
                                 ? imageInput.canRedoScreenImage
                                 : imageInput.canRedoDrawing)
                    Accessible.name: qsTr("Redo drawing change")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name + qsTr(" (Ctrl+Y)")
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.redoScreenImage()
                        else imageInput.redoDrawing()
                    }

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
                ToolButton {
                    id: brushShapeButton
                    objectName: root.objectName + "BrushShapeButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    checkable: true
                    checked: root.squareDrawingBrush
                    enabled: root.editableContentAvailable
                    Accessible.name: checked
                                     ? qsTr("Use a square drawing brush")
                                     : qsTr("Use a round drawing brush")
                    ToolTip.visible: hovered
                    ToolTip.text: checked
                                      ? qsTr("Square brush: sharp caps and rectangle corners")
                                      : qsTr("Round brush: rounded caps and corners")
                    onClicked: root.squareDrawingBrush = checked

                    contentItem: Canvas {
                        implicitWidth: 16
                        implicitHeight: 16
                        property color iconColor: brushShapeButton.enabled
                                                  ? brushShapeButton.palette.buttonText
                                                  : brushShapeButton.palette.mid
                        property bool squareState: brushShapeButton.checked
                        onIconColorChanged: requestPaint()
                        onSquareStateChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.fillStyle = iconColor
                            if (squareState)
                                context.fillRect(3, 3, 10, 10)
                            else {
                                context.beginPath()
                                context.arc(8, 8, 5, 0, Math.PI * 2)
                                context.fill()
                            }
                        }
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
                    enabled: root.editableContentAvailable
                    leftPadding: 3
                    rightPadding: 15
                    Accessible.name: qsTr("Drawing brush diameter")
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
                ToolSeparator {
                    visible: root.editingTools
                    width: visible ? 5 : 0
                    height: 26
                }
                ToolButton {
                    objectName: root.objectName + "MirrorButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("↔")
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Mirror Screen Image horizontally")
                                     : qsTr("Mirror source image horizontally")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.mirrorScreenImage()
                        else imageInput.mirrorSourceImage()
                    }
                }
                ToolButton {
                    objectName: root.objectName + "FlipButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("↕")
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Flip Screen Image vertically")
                                     : qsTr("Flip source image vertically")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.flipScreenImage()
                        else imageInput.flipSourceImage()
                    }
                }
                ToolButton {
                    objectName: root.objectName + "InvertButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("◐")
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Invert Screen Image colors")
                                     : qsTr("Invert source image colors")
                    ToolTip.visible: hovered
                    ToolTip.text: root.screenImageTools
                                  ? qsTr("Invert colors inside the selection, or the whole Screen Image when no selection is active")
                                  : qsTr("Invert colors inside the selection, or the whole source image when no selection is active")
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.invertScreenImage()
                        else imageInput.invertSourceImage()
                    }
                }
                ToolButton {
                    objectName: root.objectName + "RemoveColorButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("G")
                    font.bold: true
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Remove color from Screen Image")
                                     : qsTr("Remove color from source image")
                    ToolTip.visible: hovered
                    ToolTip.text: root.screenImageTools
                                  ? qsTr("Convert the selection to grayscale, or the whole Screen Image when no selection is active")
                                  : qsTr("Convert the selection to grayscale, or the whole source image when no selection is active")
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.removeScreenImageColor()
                        else imageInput.removeSourceImageColor()
                    }
                }
                ToolButton {
                    id: typeToolButton
                    objectName: root.objectName + "TypeToolButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("T")
                    enabled: root.editableContentAvailable
                    font.bold: true
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Place text on the Screen Image")
                                     : qsTr("Place system-font text on the source image")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.finishKLine()
                        imageInput.reloadScreenImageFonts()
                        typeToolDialog.open()
                        typeText.forceActiveFocus()
                    }
                }
                ToolButton {
                    id: clipArtToolButton
                    objectName: root.objectName + "ClipArtToolButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("▧")
                    enabled: root.editableContentAvailable
                    Accessible.name: qsTr("Place a Slide or ClipArt image")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.finishKLine()
                        clipArtDialog.open()
                    }
                }
                ToolButton {
                    id: selectionButton
                    objectName: root.objectName + "SelectionButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("▱")
                    checkable: true
                    checked: root.drawingTool === 9
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Select a rectangular Screen Image area")
                                     : qsTr("Select a rectangular source-image area")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name + qsTr("; Escape clears the selection")
                    onClicked: root.selectDrawingTool(9)
                }
                ToolButton {
                    id: moveSelectionButton
                    objectName: root.objectName + "MoveSelectionButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("✥")
                    checkable: true
                    checked: root.drawingTool === 10 && root.floatingMove
                    enabled: root.editableContentAvailable
                             && root.hasSelection
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Move the selected Screen Image area")
                                     : qsTr("Move the selected source-image area")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                                 + qsTr("; click to place or press Escape to cancel")
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools)
                            imageInput.beginMoveScreenImageSelection()
                        else
                            imageInput.beginMoveSourceSelection()
                        if (root.floating) {
                            root.drawingTool = 10
                            root.pickingColor = false
                            root.positionFloatingAtSelectionOrCenter()
                        }
                    }
                }
                ToolButton {
                    objectName: root.objectName + "CopyButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("⧉")
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Copy Screen Image")
                                     : qsTr("Copy source image")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name + qsTr(" (Ctrl+C)")
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.copyScreenImage()
                        else imageInput.copySourceImage()
                    }
                }
                ToolButton {
                    objectName: root.objectName + "PasteButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("▣")
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Paste image into Screen Image")
                                     : qsTr("Paste image into source canvas")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name + qsTr(" (Ctrl+V)")
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.pasteScreenImage()
                        else imageInput.pasteSourceImage()
                        if (root.floating) {
                            root.drawingTool = 10
                            root.pickingColor = false
                            root.positionFloatingAtSelectionOrCenter()
                        }
                    }
                }
                ToolButton {
                    objectName: root.objectName + "ClearButton"
                    visible: root.editingTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("✦")
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Clear Screen Image with the background color")
                                     : qsTr("Clear source canvas with the background color")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.clearScreenImage()
                        else imageInput.clearSourceImage()
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
                    asynchronous: !root.editingTools
                    retainWhileLoading: true
                    cache: false
                    width: Math.max(1, sourceSize.width * root.pixelAspectRatio
                                       * root.effectiveZoom)
                    height: Math.max(1, sourceSize.height * root.effectiveZoom)
                    x: Math.max(0, (flick.width - width) / 2)
                    y: Math.max(0, (flick.height - height) / 2)
                    fillMode: Image.Stretch
                    smooth: root.effectiveZoom < 1

                    Canvas {
                        id: gridOverlay
                        objectName: root.objectName + "GridOverlay"
                        anchors.fill: parent
                        z: 1
                        visible: root.screenImageTools && root.gridVisible
                                 && preview.status === Image.Ready
                        antialiasing: false
                        readonly property bool pixelGrid: root.effectiveZoom >= 5.0
                        readonly property int gridStep: pixelGrid ? 1 : 8
                        readonly property int patternGridStep: 8
                        readonly property bool drawsOuterBorder: true
                        readonly property bool patternGridVisible: visible
                        onPixelGridChanged: requestPaint()
                        onGridStepChanged: requestPaint()
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        onVisibleChanged: if (visible) requestPaint()

                        Connections {
                            target: root
                            function onEffectiveZoomChanged() {
                                gridOverlay.requestPaint()
                            }
                        }

                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            if (!visible || preview.sourceSize.width <= 0
                                    || preview.sourceSize.height <= 0)
                                return
                            context.lineWidth = 1
                            const alignedX = sourceX => {
                                if (sourceX <= 0)
                                    return 0.5
                                if (sourceX >= preview.sourceSize.width)
                                    return width - 0.5
                                return Math.round(sourceX * width
                                                  / preview.sourceSize.width) + 0.5
                            }
                            const alignedY = sourceY => {
                                if (sourceY <= 0)
                                    return 0.5
                                if (sourceY >= preview.sourceSize.height)
                                    return height - 0.5
                                return Math.round(sourceY * height
                                                  / preview.sourceSize.height) + 0.5
                            }

                            // Match the sprite tiling screen: the fine pixel grid
                            // fills only the interiors of the persistent 8x8 pattern grid.
                            if (pixelGrid) {
                                context.strokeStyle = "#70ffffff"
                                context.beginPath()
                                for (let sourceX = 1;
                                     sourceX < preview.sourceSize.width;
                                     ++sourceX) {
                                    if (sourceX % patternGridStep === 0)
                                        continue
                                    const x = alignedX(sourceX)
                                    context.moveTo(x, 0)
                                    context.lineTo(x, height)
                                }
                                for (let sourceY = 1;
                                     sourceY < preview.sourceSize.height;
                                     ++sourceY) {
                                    if (sourceY % patternGridStep === 0)
                                        continue
                                    const y = alignedY(sourceY)
                                    context.moveTo(0, y)
                                    context.lineTo(width, y)
                                }
                                context.stroke()
                            }

                            // Pattern boundaries remain visible at every zoom and
                            // include all four outer edges of the target screen.
                            context.strokeStyle = "#9effd35a"
                            context.beginPath()
                            for (let sourceX = 0;
                                 sourceX <= preview.sourceSize.width;
                                 sourceX += patternGridStep) {
                                const x = alignedX(sourceX)
                                context.moveTo(x, 0)
                                context.lineTo(x, height)
                            }
                            for (let sourceY = 0;
                                 sourceY <= preview.sourceSize.height;
                                 sourceY += patternGridStep) {
                                const y = alignedY(sourceY)
                                context.moveTo(0, y)
                                context.lineTo(width, y)
                            }
                            context.stroke()
                        }
                    }

                    Canvas {
                        id: strokeOverlay
                        objectName: root.objectName + "StrokeOverlay"
                        anchors.fill: parent
                        z: 2
                        visible: root.editingTools
                        antialiasing: !root.hardDrawingEdges
                        property var segments: []
                        property point lastPoint: Qt.point(0, 0)
                        property bool shapeActive: false
                        property bool shapeEllipse: false
                        property bool shapeLine: false
                        property real shapeStartX: 0
                        property real shapeStartY: 0
                        property real shapeEndX: 0
                        property real shapeEndY: 0
                        property bool polylineActive: false
                        property bool raysActive: false
                        property point rayOrigin: Qt.point(0, 0)
                        property real polylinePreviewX: 0
                        property real polylinePreviewY: 0
                        property bool hardEdges: root.hardDrawingEdges
                        property bool squareBrush: root.squareDrawingBrush
                        property bool shapeFilled: root.fillDrawingShapes
                        onHardEdgesChanged: requestPaint()
                        onSquareBrushChanged: requestPaint()
                        onShapeFilledChanged: requestPaint()

                        function strokeColor() {
                            return String(root.drawingTool === 2
                                          ? root.backgroundPaintColor
                                          : root.foregroundPaintColor)
                        }

                        function beginStroke(x, y) {
                            clearStrokeTimer.stop()
                            segments = []
                            shapeActive = false
                            lastPoint = Qt.point(x, y)
                            segments.push({x1: x, y1: y, x2: x, y2: y,
                                           color: strokeColor(),
                                           diameter: Math.max(1,
                                               root.drawingDiameterForTool(root.drawingTool)
                                                   * root.effectiveZoom)})
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

                        function constrainedLineEnd(startX, startY, x, y, locked) {
                            if (!locked)
                                return Qt.point(x, y)
                            if (Math.abs(x - startX) >= Math.abs(y - startY))
                                return Qt.point(x, startY)
                            return Qt.point(startX, y)
                        }

                        function beginShape(x, y, ellipse) {
                            clearStrokeTimer.stop()
                            segments = []
                            shapeActive = true
                            shapeEllipse = ellipse
                            shapeLine = false
                            shapeStartX = x
                            shapeStartY = y
                            shapeEndX = x
                            shapeEndY = y
                            requestPaint()
                        }

                        function beginLine(x, y) {
                            clearStrokeTimer.stop()
                            segments = []
                            shapeActive = true
                            shapeEllipse = false
                            shapeLine = true
                            shapeStartX = x
                            shapeStartY = y
                            shapeEndX = x
                            shapeEndY = y
                            requestPaint()
                        }

                        function updateLine(x, y, locked) {
                            const point = constrainedLineEnd(
                                shapeStartX, shapeStartY, x, y, locked)
                            shapeEndX = point.x
                            shapeEndY = point.y
                            requestPaint()
                            return point
                        }

                        function beginPolyline(x, y, rays) {
                            beginStroke(x, y)
                            polylineActive = true
                            raysActive = rays === true
                            rayOrigin = Qt.point(x, y)
                            polylinePreviewX = x
                            polylinePreviewY = y
                            requestPaint()
                        }

                        function updatePolylinePreview(x, y, locked) {
                            if (!polylineActive)
                                return Qt.point(x, y)
                            const origin = raysActive ? rayOrigin : lastPoint
                            const point = constrainedLineEnd(
                                origin.x, origin.y, x, y, locked)
                            polylinePreviewX = point.x
                            polylinePreviewY = point.y
                            requestPaint()
                            return point
                        }

                        function appendPolylinePoint(x, y, locked) {
                            const point = updatePolylinePreview(x, y, locked)
                            if (raysActive) {
                                segments.push({x1: rayOrigin.x, y1: rayOrigin.y,
                                               x2: point.x, y2: point.y,
                                               color: strokeColor(),
                                               diameter: Math.max(1,
                                                   root.drawingDiameterForTool(
                                                       root.drawingTool)
                                                       * root.effectiveZoom)})
                            } else {
                                extendStroke(point.x, point.y)
                            }
                            polylinePreviewX = point.x
                            polylinePreviewY = point.y
                            requestPaint()
                            return point
                        }

                        function finishPolyline() {
                            polylineActive = false
                            raysActive = false
                            clearStrokeTimer.restart()
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
                            shapeLine = false
                            requestPaint()
                        }

                        function extendStroke(x, y) {
                            segments.push({x1: lastPoint.x, y1: lastPoint.y,
                                           x2: x, y2: y,
                                           color: strokeColor(),
                                           diameter: Math.max(1,
                                               root.drawingDiameterForTool(root.drawingTool)
                                                   * root.effectiveZoom)})
                            lastPoint = Qt.point(x, y)
                            requestPaint()
                        }

                        function finishStroke() {
                            clearStrokeTimer.restart()
                        }

                        function clearStroke() {
                            segments = []
                            shapeActive = false
                            shapeLine = false
                            polylineActive = false
                            raysActive = false
                            requestPaint()
                        }

                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.globalAlpha = 1.0
                            context.imageSmoothingEnabled = !hardEdges
                            context.lineCap = squareBrush ? "square" : "round"
                            context.lineJoin = squareBrush ? "miter" : "round"
                            for (let index = 0; index < segments.length; ++index) {
                                const segment = segments[index]
                                context.strokeStyle = segment.color
                                context.fillStyle = segment.color
                                context.lineWidth = segment.diameter
                                if (segment.x1 === segment.x2 && segment.y1 === segment.y2) {
                                    if (squareBrush) {
                                        context.fillRect(segment.x1 - segment.diameter / 2,
                                                         segment.y1 - segment.diameter / 2,
                                                         segment.diameter, segment.diameter)
                                    } else {
                                        context.beginPath()
                                        context.arc(segment.x1, segment.y1,
                                                    segment.diameter / 2, 0, Math.PI * 2)
                                        context.fill()
                                    }
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
                                context.strokeStyle = String(root.foregroundPaintColor)
                                context.fillStyle = String(root.foregroundPaintColor)
                                context.lineWidth = Math.max(
                                    1, root.drawingDiameterForTool(root.drawingTool)
                                        * root.effectiveZoom)
                                if (!shapeLine && shapeFilled
                                        && shapeWidth >= 0.5 && shapeHeight >= 0.5) {
                                    context.fillStyle = String(root.backgroundPaintColor)
                                    if (shapeEllipse) {
                                        const fillCenterX = left + shapeWidth / 2
                                        const fillCenterY = top + shapeHeight / 2
                                        const fillRadiusX = shapeWidth / 2
                                        const fillRadiusY = shapeHeight / 2
                                        const fillSteps = Math.max(32, Math.min(4096,
                                            Math.ceil(Math.PI * (shapeWidth + shapeHeight))))
                                        context.beginPath()
                                        context.moveTo(fillCenterX + fillRadiusX,
                                                       fillCenterY)
                                        for (let step = 1; step <= fillSteps; ++step) {
                                            const angle = Math.PI * 2 * step / fillSteps
                                            context.lineTo(
                                                fillCenterX + Math.cos(angle) * fillRadiusX,
                                                fillCenterY + Math.sin(angle) * fillRadiusY)
                                        }
                                        context.closePath()
                                        context.fill()
                                    } else {
                                        context.fillRect(left, top, shapeWidth, shapeHeight)
                                    }
                                }
                                context.strokeStyle = String(root.foregroundPaintColor)
                                context.fillStyle = String(root.foregroundPaintColor)
                                context.beginPath()
                                if (shapeWidth < 0.5 && shapeHeight < 0.5) {
                                    if (squareBrush) {
                                        context.fillRect(
                                            shapeStartX - context.lineWidth / 2,
                                            shapeStartY - context.lineWidth / 2,
                                            context.lineWidth, context.lineWidth)
                                    } else {
                                        context.arc(shapeStartX, shapeStartY,
                                                    context.lineWidth / 2,
                                                    0, Math.PI * 2)
                                        context.fill()
                                    }
                                } else if (shapeLine) {
                                    context.moveTo(shapeStartX, shapeStartY)
                                    context.lineTo(shapeEndX, shapeEndY)
                                    context.stroke()
                                } else if (shapeEllipse) {
                                    const centerX = left + shapeWidth / 2
                                    const centerY = top + shapeHeight / 2
                                    const radiusX = shapeWidth / 2
                                    const radiusY = shapeHeight / 2
                                    const steps = Math.max(32, Math.min(4096,
                                        Math.ceil(Math.PI * (shapeWidth + shapeHeight))))
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
                            }
                            if (polylineActive) {
                                context.strokeStyle = strokeColor()
                                context.lineWidth = Math.max(
                                    1, root.drawingDiameterForTool(root.drawingTool)
                                        * root.effectiveZoom)
                                context.lineCap = squareBrush ? "square" : "round"
                                context.beginPath()
                                context.moveTo(raysActive ? rayOrigin.x : lastPoint.x,
                                               raysActive ? rayOrigin.y : lastPoint.y)
                                context.lineTo(polylinePreviewX, polylinePreviewY)
                                context.stroke()
                            }
                        }

                        Timer {
                            id: clearStrokeTimer
                            interval: 40
                            repeat: false
                            onTriggered: strokeOverlay.clearStroke()
                        }
                    }

                    Canvas {
                        id: selectionOverlay
                        objectName: root.objectName + "SelectionOverlay"
                        anchors.fill: parent
                        z: 3
                        visible: root.editingTools
                                 && ((root.hasSelection && !root.floating)
                                     || drawingMouseArea.selectionDragging)
                        antialiasing: false
                        property int selectionX: root.selectionX
                        property int selectionY: root.selectionY
                        property int selectionWidth: root.selectionWidth
                        property int selectionHeight: root.selectionHeight
                        property bool dragging: drawingMouseArea.selectionDragging
                        onSelectionXChanged: requestPaint()
                        onSelectionYChanged: requestPaint()
                        onSelectionWidthChanged: requestPaint()
                        onSelectionHeightChanged: requestPaint()
                        onDraggingChanged: requestPaint()
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()

                        function paintOutline(context, x, y, width, height) {
                            context.lineWidth = 3
                            context.strokeStyle = "#d0101010"
                            context.setLineDash([])
                            context.strokeRect(x, y, width, height)
                            context.lineWidth = 1
                            context.strokeStyle = "#ffffff"
                            context.setLineDash([4, 3])
                            context.strokeRect(x, y, width, height)
                            context.setLineDash([])
                        }

                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            if (!visible || preview.sourceSize.width <= 0
                                    || preview.sourceSize.height <= 0)
                                return
                            if (dragging) {
                                const left = Math.min(drawingMouseArea.selectionStartX,
                                                      drawingMouseArea.selectionCurrentX)
                                const top = Math.min(drawingMouseArea.selectionStartY,
                                                     drawingMouseArea.selectionCurrentY)
                                const selectionWidth = Math.max(
                                    1, Math.abs(drawingMouseArea.selectionCurrentX
                                                - drawingMouseArea.selectionStartX))
                                const selectionHeight = Math.max(
                                    1, Math.abs(drawingMouseArea.selectionCurrentY
                                                - drawingMouseArea.selectionStartY))
                                paintOutline(context, left, top,
                                             selectionWidth, selectionHeight)
                            } else if (root.hasSelection) {
                                paintOutline(
                                    context,
                                    selectionX * width / preview.sourceSize.width,
                                    selectionY * height / preview.sourceSize.height,
                                    selectionWidth * width / preview.sourceSize.width,
                                    selectionHeight * height / preview.sourceSize.height)
                            }
                        }
                    }

                    Item {
                        id: floatingPlacementOverlay
                        objectName: root.objectName + "FloatingPlacementOverlay"
                        z: 4
                        visible: root.editingTools && root.floating
                                 && preview.sourceSize.width > 0
                                 && preview.sourceSize.height > 0
                        width: visible ? Math.max(
                            1, root.floatingWidth
                               * preview.width / preview.sourceSize.width) : 1
                        height: visible ? Math.max(
                            1, root.floatingHeight
                               * preview.height / preview.sourceSize.height) : 1
                        x: Math.max(0, Math.min(preview.width - width,
                                              drawingMouseArea.floatingCenterX
                                              - (root.floatingTopLeft
                                                 ? 0 : width / 2)))
                        y: Math.max(0, Math.min(preview.height - height,
                                              drawingMouseArea.floatingCenterY
                                              - (root.floatingTopLeft
                                                 ? 0 : height / 2)))

                        Image {
                            anchors.fill: parent
                            source: root.floatingPreview
                            fillMode: Image.Stretch
                            smooth: false
                            opacity: 0.88
                        }

                        Canvas {
                            anchors.fill: parent
                            onWidthChanged: requestPaint()
                            onHeightChanged: requestPaint()
                            onPaint: {
                                const context = getContext("2d")
                                context.clearRect(0, 0, width, height)
                                context.lineWidth = 3
                                context.strokeStyle = "#d0101010"
                                context.strokeRect(0, 0, width, height)
                                context.lineWidth = 1
                                context.strokeStyle = "#ffdd55"
                                context.setLineDash([4, 3])
                                context.strokeRect(0, 0, width, height)
                                context.setLineDash([])
                            }
                        }
                    }

                    MouseArea {
                        id: drawingMouseArea
                        z: 5
                        anchors.fill: parent
                        enabled: root.editingTools && root.editableContentAvailable
                                 && (root.drawingTool !== 0
                                     || root.floating)
                        acceptedButtons: Qt.LeftButton
                        hoverEnabled: true
                        preventStealing: true
                        cursorShape: root.floating
                                     ? Qt.SizeAllCursor : Qt.CrossCursor
                        property int dragTool: 0
                        property bool selectionDragging: false
                        property real selectionStartX: 0
                        property real selectionStartY: 0
                        property real selectionCurrentX: 0
                        property real selectionCurrentY: 0
                        property real selectionAnchorStartX: 0
                        property real selectionAnchorStartY: 0
                        property real selectionAnchorEndX: 0
                        property real selectionAnchorEndY: 0
                        property real floatingCenterX: width / 2
                        property real floatingCenterY: height / 2
                        function updateFloatingPointer(localX, localY) {
                            if (root.floatingTopLeft
                                    && root.snapDrawingToCharacterBounds
                                    && preview.sourceSize.width > 0
                                    && preview.sourceSize.height > 0) {
                                const sourceX = Math.max(
                                    0, Math.min(preview.sourceSize.width - 0.001,
                                                localX * preview.sourceSize.width
                                                / Math.max(1, width)))
                                const sourceY = Math.max(
                                    0, Math.min(preview.sourceSize.height - 0.001,
                                                localY * preview.sourceSize.height
                                                / Math.max(1, height)))
                                floatingCenterX = Math.floor(sourceX / 8) * 8
                                    * width / preview.sourceSize.width
                                floatingCenterY = Math.floor(sourceY / 8) * 8
                                    * height / preview.sourceSize.height
                            } else {
                                floatingCenterX = localX
                                floatingCenterY = localY
                            }
                        }
                        focus: strokeOverlay.polylineActive
                               || root.floating || root.hasSelection
                        Keys.onEscapePressed: event => {
                            if (root.floating) {
                                if (root.screenImageTools)
                                    imageInput.cancelScreenImageFloating()
                                else
                                    imageInput.cancelSourceFloating()
                                root.drawingTool = root.hasSelection ? 9 : 0
                                dragTool = 0
                                event.accepted = true
                            } else if (strokeOverlay.polylineActive) {
                                root.finishKLine()
                                dragTool = 0
                                event.accepted = true
                            } else if (root.hasSelection) {
                                if (root.screenImageTools)
                                    imageInput.clearScreenImageSelection()
                                else
                                    imageInput.clearSourceSelection()
                                event.accepted = true
                            }
                        }
                        function beginSelectionPointer(localX, localY) {
                            const start = root.characterSelectionPoint(
                                localX, localY, false)
                            const end = root.characterSelectionPoint(
                                localX, localY, true)
                            selectionAnchorStartX = start.x
                            selectionAnchorStartY = start.y
                            selectionAnchorEndX = end.x
                            selectionAnchorEndY = end.y
                            selectionStartX = start.x
                            selectionStartY = start.y
                            selectionCurrentX = end.x
                            selectionCurrentY = end.y
                        }
                        function updateSelectionPointer(localX, localY) {
                            const start = root.characterSelectionPoint(
                                localX, localY, false)
                            const end = root.characterSelectionPoint(
                                localX, localY, true)
                            selectionStartX = start.x < selectionAnchorStartX
                                ? selectionAnchorEndX : selectionAnchorStartX
                            selectionCurrentX = start.x < selectionAnchorStartX
                                ? start.x : end.x
                            selectionStartY = start.y < selectionAnchorStartY
                                ? selectionAnchorEndY : selectionAnchorStartY
                            selectionCurrentY = start.y < selectionAnchorStartY
                                ? start.y : end.y
                        }
                        onPressed: mouse => {
                            root.editingSurfaceActivated(root.screenImageTools)
                            updateFloatingPointer(mouse.x, mouse.y)
                            if (root.floating) {
                                if (root.screenImageTools) {
                                    imageInput.placeScreenImageFloating(
                                        floatingCenterX / Math.max(1, width),
                                        floatingCenterY / Math.max(1, height))
                                } else {
                                    imageInput.placeSourceFloating(
                                        floatingCenterX / Math.max(1, width),
                                        floatingCenterY / Math.max(1, height))
                                }
                                root.drawingTool = 9
                                dragTool = 0
                                return
                            }
                            dragTool = root.drawingTool
                            if (dragTool === 9 && root.editingTools) {
                                forceActiveFocus()
                                selectionDragging = true
                                beginSelectionPointer(mouse.x, mouse.y)
                                selectionOverlay.requestPaint()
                            } else if (dragTool <= 2) {
                                strokeOverlay.beginStroke(mouse.x, mouse.y)
                                if (root.screenImageTools) {
                                    imageInput.beginScreenImageStroke(
                                        mouse.x / Math.max(1, width),
                                        mouse.y / Math.max(1, height),
                                        root.drawingDiameterForTool(dragTool),
                                        dragTool === 2,
                                        root.hardDrawingEdges,
                                        root.squareDrawingBrush)
                                } else {
                                    imageInput.beginSourceStroke(
                                        mouse.x / Math.max(1, width),
                                        mouse.y / Math.max(1, height),
                                        root.drawingDiameterForTool(dragTool),
                                        dragTool === 2,
                                        root.hardDrawingEdges,
                                        root.squareDrawingBrush)
                                }
                            } else if (dragTool === 8) {
                                if (root.screenImageTools) {
                                    imageInput.swapScreenImageColors(
                                        mouse.x / Math.max(1, width),
                                        mouse.y / Math.max(1, height))
                                } else {
                                    imageInput.swapSourceColors(
                                        mouse.x / Math.max(1, width),
                                        mouse.y / Math.max(1, height))
                                }
                            } else if (dragTool === 5) {
                                const start = root.drawingStartPoint(
                                    mouse.x, mouse.y, dragTool)
                                strokeOverlay.beginLine(start.x, start.y)
                                const end = root.drawingEndPoint(
                                    mouse.x, mouse.y, dragTool)
                                strokeOverlay.updateLine(end.x, end.y, false)
                            } else if (dragTool === 6 || dragTool === 7) {
                                forceActiveFocus()
                                if (!strokeOverlay.polylineActive) {
                                    const start = root.drawingStartPoint(
                                        mouse.x, mouse.y, dragTool)
                                    strokeOverlay.beginPolyline(
                                        start.x, start.y, dragTool === 7)
                                    const end = root.drawingEndPoint(
                                        mouse.x, mouse.y, dragTool)
                                    strokeOverlay.updatePolylinePreview(
                                        end.x, end.y, false)
                                    if (root.screenImageTools) {
                                        imageInput.beginScreenImageStroke(
                                            start.x / Math.max(1, width),
                                            start.y / Math.max(1, height),
                                            root.drawingDiameterForTool(dragTool), false,
                                            root.hardDrawingEdges,
                                            root.squareDrawingBrush)
                                    } else {
                                        imageInput.beginSourceStroke(
                                            start.x / Math.max(1, width),
                                            start.y / Math.max(1, height),
                                            root.drawingDiameterForTool(dragTool), false,
                                            root.hardDrawingEdges,
                                            root.squareDrawingBrush)
                                    }
                                } else {
                                    const end = root.drawingEndPoint(
                                        mouse.x, mouse.y, dragTool)
                                    const point = strokeOverlay.appendPolylinePoint(
                                        end.x, end.y,
                                        (mouse.modifiers & Qt.ShiftModifier) !== 0)
                                    if (root.screenImageTools) {
                                        if (dragTool === 7)
                                            imageInput.continueScreenImageRay(
                                                point.x / Math.max(1, width),
                                                point.y / Math.max(1, height))
                                        else
                                            imageInput.continueScreenImageStroke(
                                                point.x / Math.max(1, width),
                                                point.y / Math.max(1, height))
                                    } else {
                                        if (dragTool === 7)
                                            imageInput.continueSourceRay(
                                                point.x / Math.max(1, width),
                                                point.y / Math.max(1, height))
                                        else
                                            imageInput.continueSourceStroke(
                                                point.x / Math.max(1, width),
                                                point.y / Math.max(1, height))
                                    }
                                }
                            } else if (dragTool === 3 || dragTool === 4) {
                                const start = root.drawingStartPoint(
                                    mouse.x, mouse.y, dragTool)
                                const end = root.drawingEndPoint(
                                    mouse.x, mouse.y, dragTool)
                                strokeOverlay.beginShape(start.x, start.y,
                                                         dragTool === 3)
                                strokeOverlay.updateShape(end.x, end.y, false)
                            }
                        }
                        onPositionChanged: mouse => {
                            updateFloatingPointer(mouse.x, mouse.y)
                            if (strokeOverlay.polylineActive
                                    && (root.drawingTool === 6
                                        || root.drawingTool === 7)) {
                                const end = root.drawingEndPoint(
                                    mouse.x, mouse.y, root.drawingTool)
                                strokeOverlay.updatePolylinePreview(
                                    end.x, end.y,
                                    (mouse.modifiers & Qt.ShiftModifier) !== 0)
                            }
                            if (pressed) {
                                if (dragTool === 9 && selectionDragging) {
                                    updateSelectionPointer(mouse.x, mouse.y)
                                    selectionOverlay.requestPaint()
                                } else if (dragTool <= 2) {
                                    strokeOverlay.extendStroke(mouse.x, mouse.y)
                                    if (root.screenImageTools) {
                                        imageInput.continueScreenImageStroke(
                                            mouse.x / Math.max(1, width),
                                            mouse.y / Math.max(1, height))
                                    } else {
                                        imageInput.continueSourceStroke(
                                            mouse.x / Math.max(1, width),
                                            mouse.y / Math.max(1, height))
                                    }
                                } else if (dragTool === 5) {
                                    const end = root.drawingEndPoint(
                                        mouse.x, mouse.y, dragTool)
                                    strokeOverlay.updateLine(
                                        end.x, end.y,
                                        (mouse.modifiers & Qt.ShiftModifier) !== 0)
                                } else if (dragTool === 3 || dragTool === 4) {
                                    const end = root.drawingEndPoint(
                                        mouse.x, mouse.y, dragTool)
                                    strokeOverlay.updateShape(
                                        end.x, end.y,
                                        (mouse.modifiers & Qt.ShiftModifier) !== 0)
                                }
                            }
                        }
                        onReleased: mouse => {
                            if (dragTool === 9 && selectionDragging) {
                                updateSelectionPointer(mouse.x, mouse.y)
                                if (root.screenImageTools) {
                                    imageInput.setScreenImageSelection(
                                        selectionStartX / Math.max(1, width),
                                        selectionStartY / Math.max(1, height),
                                        selectionCurrentX / Math.max(1, width),
                                        selectionCurrentY / Math.max(1, height))
                                } else {
                                    imageInput.setSourceSelection(
                                        selectionStartX / Math.max(1, width),
                                        selectionStartY / Math.max(1, height),
                                        selectionCurrentX / Math.max(1, width),
                                        selectionCurrentY / Math.max(1, height))
                                }
                                selectionDragging = false
                                selectionOverlay.requestPaint()
                                forceActiveFocus()
                            } else if (dragTool <= 2) {
                                if (root.screenImageTools) imageInput.endScreenImageStroke()
                                else imageInput.endSourceStroke()
                                strokeOverlay.finishStroke()
                            } else if (dragTool === 5) {
                                const end = root.drawingEndPoint(
                                    mouse.x, mouse.y, dragTool)
                                strokeOverlay.updateLine(
                                    end.x, end.y,
                                    (mouse.modifiers & Qt.ShiftModifier) !== 0)
                                if (root.screenImageTools) {
                                    imageInput.drawScreenImageLine(
                                        strokeOverlay.shapeStartX / Math.max(1, width),
                                        strokeOverlay.shapeStartY / Math.max(1, height),
                                        strokeOverlay.shapeEndX / Math.max(1, width),
                                        strokeOverlay.shapeEndY / Math.max(1, height),
                                        root.drawingDiameterForTool(dragTool),
                                        root.hardDrawingEdges,
                                        root.squareDrawingBrush)
                                } else {
                                    imageInput.drawSourceLine(
                                        strokeOverlay.shapeStartX / Math.max(1, width),
                                        strokeOverlay.shapeStartY / Math.max(1, height),
                                        strokeOverlay.shapeEndX / Math.max(1, width),
                                        strokeOverlay.shapeEndY / Math.max(1, height),
                                        root.drawingDiameterForTool(dragTool),
                                        root.hardDrawingEdges,
                                        root.squareDrawingBrush)
                                }
                                strokeOverlay.finishShape()
                            } else if (dragTool === 3 || dragTool === 4) {
                                const end = root.drawingEndPoint(
                                    mouse.x, mouse.y, dragTool)
                                strokeOverlay.updateShape(
                                    end.x, end.y,
                                    (mouse.modifiers & Qt.ShiftModifier) !== 0)
                                if (root.screenImageTools) {
                                    imageInput.drawScreenImageShape(
                                        strokeOverlay.shapeStartX / Math.max(1, width),
                                        strokeOverlay.shapeStartY / Math.max(1, height),
                                        strokeOverlay.shapeEndX / Math.max(1, width),
                                        strokeOverlay.shapeEndY / Math.max(1, height),
                                        root.drawingDiameterForTool(dragTool),
                                        dragTool === 3,
                                        root.hardDrawingEdges,
                                        root.fillDrawingShapes,
                                        root.squareDrawingBrush)
                                } else {
                                    imageInput.drawSourceShape(
                                        strokeOverlay.shapeStartX / Math.max(1, width),
                                        strokeOverlay.shapeStartY / Math.max(1, height),
                                        strokeOverlay.shapeEndX / Math.max(1, width),
                                        strokeOverlay.shapeEndY / Math.max(1, height),
                                        root.drawingDiameterForTool(dragTool),
                                        dragTool === 3,
                                        root.hardDrawingEdges,
                                        root.fillDrawingShapes,
                                        root.squareDrawingBrush)
                                }
                                strokeOverlay.finishShape()
                            }
                            dragTool = 0
                        }
                        onCanceled: {
                            if (dragTool === 9) {
                                selectionDragging = false
                                selectionOverlay.requestPaint()
                            } else if (dragTool <= 2) {
                                if (root.screenImageTools) imageInput.endScreenImageStroke()
                                else imageInput.endSourceStroke()
                                strokeOverlay.finishStroke()
                            } else if (dragTool === 6 || dragTool === 7) {
                                root.finishMultiLine()
                            } else if (dragTool === 3 || dragTool === 4
                                       || dragTool === 5) {
                                strokeOverlay.cancelShape()
                            }
                            dragTool = 0
                        }
                    }

                    Item {
                        id: brushCursorRing
                        objectName: root.objectName + "BrushCursorRing"
                        z: 6
                        readonly property real cursorDiameter: Math.max(
                            1, root.drawingDiameterForTool(root.drawingTool)
                                * root.effectiveZoom)
                        width: Math.max(3, cursorDiameter)
                        height: width
                        x: drawingMouseArea.mouseX - width / 2
                        y: drawingMouseArea.mouseY - height / 2
                        visible: drawingMouseArea.enabled
                                 && drawingMouseArea.containsMouse
                                 && root.drawingTool > 0
                                 && root.drawingTool < 8

                        Rectangle {
                            anchors.fill: parent
                            radius: root.squareDrawingBrush ? 0 : width / 2
                            color: "transparent"
                            border.width: 3
                            border.color: "#202020"
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: root.squareDrawingBrush ? 0 : width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: "#f5f5f5"
                        }
                    }

                    TapHandler {
                        enabled: root.editingTools && root.pickingColor
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
                        enabled: root.editingTools && root.pickingColor
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
                source: Qt.resolvedUrl("../assets/icons/RetroVDPStudio-watermark-512.png")
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                opacity: 0.12
                visible: root.imageSource.toString().length === 0 && !root.busy
                Accessible.name: qsTr("RetroVDP Studio logo")
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
                objectName: root.objectName + "WheelZoomHandler"
                enabled: root.zoomInteractive && root.zoomContentAvailable
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                objectName: root.objectName + "Details"
                Layout.fillWidth: true
                text: root.details
                wrapMode: Text.WordWrap
                color: palette.placeholderText
            }
            Label {
                objectName: root.objectName + "DetailsTrailing"
                visible: text.length > 0
                text: root.detailsTrailingText
                color: palette.placeholderText
                horizontalAlignment: Text.AlignRight
            }
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
                visible: root.editingTools
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: childrenRect.height
                spacing: 1

                Item {
                    id: colorControl
                    objectName: root.objectName + "BackgroundColorButton"
                    implicitWidth: 26
                    implicitHeight: 26
                    enabled: root.editableContentAvailable
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
                        color: root.backgroundPaintColor
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
                        color: root.foregroundPaintColor
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
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Pick the active color from the Screen Image")
                                     : qsTr("Pick the active color from the source image")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.pickingColor = checked
                        if (checked) {
                            root.finishKLine()
                            root.drawingTool = 0
                        }
                    }
                }
                ToolSeparator { width: 5; height: 26 }
                ToolButton {
                    objectName: root.objectName + "MoveLeftButton"
                    implicitWidth: 24
                    implicitHeight: 26
                    text: qsTr("←")
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Move Screen Image left")
                                     : qsTr("Move source left")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.nudgeScreenImage(-1, 0)
                        else imageInput.nudgeSource(-1, 0)
                    }
                }
                ToolButton {
                    objectName: root.objectName + "MoveUpButton"
                    implicitWidth: 24
                    implicitHeight: 26
                    text: qsTr("↑")
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Move Screen Image up")
                                     : qsTr("Move source up")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.nudgeScreenImage(0, -1)
                        else imageInput.nudgeSource(0, -1)
                    }
                }
                ToolButton {
                    objectName: root.objectName + "CenterButton"
                    visible: !root.screenImageTools
                    implicitWidth: 24
                    implicitHeight: 26
                    text: qsTr("◎")
                    enabled: root.editableContentAvailable
                             && (imageInput.horizontalOffset !== 0
                                 || imageInput.verticalOffset !== 0)
                    Accessible.name: qsTr("Center source")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.finishKLine()
                        imageInput.centerSource()
                    }
                }
                ToolButton {
                    objectName: root.objectName + "MoveDownButton"
                    implicitWidth: 24
                    implicitHeight: 26
                    text: qsTr("↓")
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Move Screen Image down")
                                     : qsTr("Move source down")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.nudgeScreenImage(0, 1)
                        else imageInput.nudgeSource(0, 1)
                    }
                }
                ToolButton {
                    objectName: root.objectName + "MoveRightButton"
                    implicitWidth: 24
                    implicitHeight: 26
                    text: qsTr("→")
                    enabled: root.editableContentAvailable
                    Accessible.name: root.screenImageTools
                                     ? qsTr("Move Screen Image right")
                                     : qsTr("Move source right")
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.finishKLine()
                        if (root.screenImageTools) imageInput.nudgeScreenImage(1, 0)
                        else imageInput.nudgeSource(1, 0)
                    }
                }
                ToolSeparator {
                    visible: root.screenImageTools
                    width: visible ? 5 : 0
                    height: 26
                }
                ToolButton {
                    objectName: root.objectName + "GridButton"
                    visible: root.screenImageTools
                    implicitWidth: 26
                    implicitHeight: 26
                    text: qsTr("#")
                    checkable: true
                    checked: root.gridVisible
                    enabled: root.editableContentAvailable
                    Accessible.name: checked
                                     ? qsTr("Hide Screen Image grid")
                                     : qsTr("Show Screen Image grid")
                    ToolTip.visible: hovered
                    ToolTip.text: root.effectiveZoom >= 5.0
                                  ? qsTr("Pixel and 8 by 8 pattern grid")
                                  : qsTr("8 by 8 character grid")
                    onClicked: root.gridVisible = checked
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

                ToolButton {
                    objectName: root.convertedTools
                                ? root.objectName + "ApplyScreenImageEditsButton" : ""
                    implicitHeight: 26
                    text: qsTr("Apply")
                    enabled: root.screenImageTools && imageInput.screenImageEdited
                             && !imageInput.busy
                    Accessible.name: qsTr("Apply chipset rules to Screen Image drawing")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Reconvert the complete edited Screen Image with the active chipset mode; this is undoable")
                    onClicked: {
                        root.finishKLine()
                        imageInput.applyScreenImageEdits()
                    }
                }
            }

            Item {
                visible: !root.editingTools && !root.convertedTools
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
                    enabled: root.zoomInteractive && root.zoomContentAvailable
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
                        enabled: root.zoomInteractive && root.zoomContentAvailable
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
                        enabled: root.zoomInteractive && root.zoomContentAvailable
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

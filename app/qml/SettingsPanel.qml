import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth
    property bool closable: false
    signal exportRequested()
    signal closeRequested()
    property int editedPaletteIndex: -1

    ColorDialog {
        id: paletteEditor
        title: qsTr("Choose working-palette color")
        onAccepted: imageInput.setWorkingPaletteColor(root.editedPaletteIndex,
                                                       selectedColor)
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Image Settings")
                font.pixelSize: 20
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            Switch {
                objectName: "livePreviewSwitch"
                text: qsTr("Live")
                checked: imageInput.livePreview
                activeFocusOnTab: true
                Accessible.name: qsTr("Show live conversion progress")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Draw completed rows while converting")
                onToggled: imageInput.livePreview = checked
            }
            ToolButton {
                objectName: "updateConversionButton"
                implicitWidth: 28
                implicitHeight: 28
                text: qsTr("↻")
                enabled: imageInput.hasImage && !imageInput.busy
                         && (!imageInput.autoUpdate || imageInput.conversionPending)
                activeFocusOnTab: true
                Accessible.name: qsTr("Update Screen Image preview")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: imageInput.updateConversion()
            }
            ToolButton {
                objectName: root.closable ? "overlayHideButton" : "adjacentHideButton"
                visible: root.closable
                text: qsTr("›")
                font.pixelSize: 24
                activeFocusOnTab: true
                Accessible.name: qsTr("Hide Side Panel")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: root.closeRequested()
            }
        }

        ThemedGroupBox {
            objectName: "screenImageOutputProfilesGroup"
            title: qsTr("Output Profiles")
            Layout.fillWidth: true

            OutputProfileControls {
                anchors.fill: parent
                objectPrefix: "screenImageOutputProfiles"
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            ToolButton {
                id: artStyleToggle
                objectName: "artStyleSettingsToggle"
                text: checked ? qsTr("▾ Art Style") : qsTr("▸ Art Style")
                checkable: true
                checked: true
                Layout.fillWidth: true
                palette.buttonText: "#17324d"
                background: Rectangle {
                    objectName: "artStyleSettingsToggleBackground"
                    radius: 3
                    color: artStyleToggle.down ? "#78bce8"
                                               : artStyleToggle.hovered ? "#a8d7f5"
                                                                      : "#bfe3f8"
                    border.width: 1
                    border.color: "#69a9d1"
                }
                Accessible.name: qsTr("Show art style presets")
                activeFocusOnTab: true
            }

            ThemedGroupBox {
                objectName: "recommendedSettingsGroup"
                visible: artStyleToggle.checked
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    ComboBox {
                        id: presetBox
                        Layout.fillWidth: true
                        model: [
                            qsTr("Balanced"),
                            qsTr("Crisp pixel art"),
                            qsTr("Smooth photograph"),
                            qsTr("Ordered retro")
                        ]
                        Accessible.name: qsTr("Conversion preset")
                        activeFocusOnTab: true
                    }
                    Button {
                        text: qsTr("Apply preset")
                        Layout.fillWidth: true
                        activeFocusOnTab: true
                        Accessible.name: text
                        onClicked: imageInput.applyPreset(presetBox.currentIndex)
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            ToolButton {
                id: commonToggle
                objectName: "commonSettingsToggle"
                text: checked ? qsTr("▾ Common settings") : qsTr("▸ Common settings")
                checkable: true
                Layout.fillWidth: true
                palette.buttonText: "#17324d"
                background: Rectangle {
                    objectName: "commonSettingsToggleBackground"
                    radius: 3
                    color: commonToggle.down ? "#78bce8"
                                             : commonToggle.hovered ? "#a8d7f5"
                                                                    : "#bfe3f8"
                    border.width: 1
                    border.color: "#69a9d1"
                }
                Accessible.name: qsTr("Show common conversion settings")
                activeFocusOnTab: true
            }

            ThemedGroupBox {
                objectName: "commonSettingsGroup"
                visible: commonToggle.checked
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    Label { text: qsTr("Target mode") }
                    ComboBox {
                        Layout.fillWidth: true
                        model: [
                            qsTr("Bitmap 9918A"),
                            qsTr("Greyscale Bitmap 9918A"),
                            qsTr("B&W Bitmap 9918A"),
                            qsTr("Multicolor 9918"),
                            qsTr("Dual Multicolor 9918"),
                            qsTr("Half Multicolor 9918A"),
                            qsTr("Bitmap Color Only 9918A"),
                            qsTr("Paletted Bitmap F18A"),
                            qsTr("Scanline Palette Bitmap F18A")
                        ]
                        currentIndex: imageInput.conversionMode
                        onActivated: imageInput.conversionMode = currentIndex
                        Accessible.name: qsTr("Target conversion mode")
                        activeFocusOnTab: true
                    }

                    Label { text: qsTr("Dithering") }
                    ComboBox {
                        objectName: "ditherModeCombo"
                        Layout.fillWidth: true
                        model: [
                            qsTr("None"),
                            qsTr("Floyd–Steinberg"),
                            qsTr("Atkinson"),
                            qsTr("Pattern"),
                            qsTr("Diagonal"),
                            qsTr("Ordered"),
                            qsTr("Ordered with error"),
                            qsTr("Custom")
                        ]
                        currentIndex: imageInput.ditherMode
                        onActivated: imageInput.ditherMode = currentIndex
                        Accessible.name: qsTr("Dithering method")
                        activeFocusOnTab: true
                    }

                    CheckBox {
                        text: qsTr("Perceptual color matching")
                        checked: imageInput.perceptualColorMatching
                        onToggled: imageInput.perceptualColorMatching = checked
                        activeFocusOnTab: true
                    }
                    CheckBox {
                        text: qsTr("Stretch histogram")
                        checked: imageInput.stretchHistogram
                        onToggled: imageInput.stretchHistogram = checked
                        activeFocusOnTab: true
                    }
                    CheckBox {
                        objectName: "powerPaintFramingCheckBox"
                        text: qsTr("PowerPaint 240×160 active area")
                        checked: imageInput.powerPaintFraming
                        onToggled: imageInput.powerPaintFraming = checked
                        activeFocusOnTab: true
                        Accessible.name: text
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            ToolButton {
                id: framingToggle
                objectName: "framingSettingsToggle"
                text: checked ? qsTr("▾ Framing and scale") : qsTr("▸ Framing and scale")
                checkable: true
                Layout.fillWidth: true
                palette.buttonText: "#17324d"
                background: Rectangle {
                    objectName: "framingSettingsToggleBackground"
                    radius: 3
                    color: framingToggle.down ? "#78bce8"
                                              : framingToggle.hovered ? "#a8d7f5"
                                                                      : "#bfe3f8"
                    border.width: 1
                    border.color: "#69a9d1"
                }
                Accessible.name: qsTr("Show framing and scale settings")
                activeFocusOnTab: true
            }

            ThemedGroupBox {
                objectName: "framingSettingsGroup"
                visible: framingToggle.checked
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    Label { text: qsTr("Framing") }
                    ComboBox {
                        Layout.fillWidth: true
                        model: [qsTr("Fit entire image"), qsTr("Crop from start"),
                                qsTr("Crop from center"), qsTr("Crop from end")]
                        currentIndex: imageInput.fillMode
                        onActivated: imageInput.fillMode = currentIndex
                        Accessible.name: qsTr("Crop and fill mode")
                        activeFocusOnTab: true
                    }
                    Label { text: qsTr("Scaling filter") }
                    ComboBox {
                        Layout.fillWidth: true
                        model: [qsTr("Box"), qsTr("Gaussian"), qsTr("Hamming"),
                                qsTr("Blackman"), qsTr("Bilinear"), qsTr("Nearest / none")]
                        currentIndex: imageInput.scalingFilter
                        onActivated: imageInput.scalingFilter = currentIndex
                        Accessible.name: qsTr("Scaling filter")
                        activeFocusOnTab: true
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            ToolButton {
                id: advancedToggle
                objectName: "advancedSettingsToggle"
                text: checked ? qsTr("▾ Advanced settings") : qsTr("▸ Advanced settings")
                checkable: true
                Layout.fillWidth: true
                palette.buttonText: "#17324d"
                background: Rectangle {
                    objectName: "advancedSettingsToggleBackground"
                    radius: 3
                    color: advancedToggle.down ? "#78bce8"
                                               : advancedToggle.hovered ? "#a8d7f5"
                                                                        : "#bfe3f8"
                    border.width: 1
                    border.color: "#69a9d1"
                }
                Accessible.name: qsTr("Show advanced conversion settings")
                activeFocusOnTab: true
            }

            ThemedGroupBox {
                objectName: "advancedSettingsGroup"
                visible: advancedToggle.checked
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    Label { text: qsTr("Distributed error handling") }
                    ComboBox {
                        objectName: "errorAccumulationCombo"
                        Layout.fillWidth: true
                        model: [qsTr("Average"), qsTr("Accumulate")]
                        currentIndex: imageInput.errorAccumulationMode
                        onActivated: imageInput.errorAccumulationMode = currentIndex
                        Accessible.name: qsTr("Distributed error handling")
                        activeFocusOnTab: true
                    }
                    Label {
                        text: imageInput.ditherMode === 5
                              ? qsTr("Ordered pattern (legacy Order %1)")
                                    .arg(imageInput.orderedDitherMapSize === 4 ? 3 : 1)
                              : imageInput.ditherMode === 6
                                ? qsTr("Ordered pattern (legacy Order %1)")
                                      .arg(imageInput.orderedDitherMapSize === 4 ? 4 : 2)
                                : qsTr("Ordered pattern")
                        color: orderedMapSizeCombo.enabled ? palette.text
                                                           : palette.placeholderText
                    }
                    ComboBox {
                        id: orderedMapSizeCombo
                        objectName: "orderedDitherMapSizeCombo"
                        Layout.fillWidth: true
                        model: [qsTr("2×2 — legacy Order 1 / Order 2"),
                                qsTr("4×4 — legacy Order 3 / Order 4")]
                        currentIndex: imageInput.orderedDitherMapSize === 4 ? 1 : 0
                        enabled: imageInput.ditherMode === 5 || imageInput.ditherMode === 6
                        onActivated: imageInput.orderedDitherMapSize = currentIndex === 1 ? 4 : 2
                        Accessible.name: qsTr("Ordered dither pattern size")
                        activeFocusOnTab: true
                    }
                    Label {
                        text: qsTr("Error distribution weights (sixteenths)")
                        color: errorWeightGrid.enabled ? palette.text
                                                       : palette.placeholderText
                    }
                    GridLayout {
                        id: errorWeightGrid
                        objectName: "errorWeightGrid"
                        Layout.fillWidth: true
                        columns: 6
                        columnSpacing: 3
                        rowSpacing: 2
                        enabled: imageInput.ditherMode !== 0 && imageInput.ditherMode !== 5

                        Label { text: qsTr("↙") }
                        SpinBox {
                            objectName: "errorDownLeftSpinBox"
                            Layout.preferredWidth: 54
                            from: 0
                            to: 16
                            editable: true
                            value: imageInput.errorDownLeft
                            onValueModified: imageInput.errorDownLeft = value
                            Accessible.name: qsTr("Error weight down-left")
                        }
                        Label { text: qsTr("↓") }
                        SpinBox {
                            objectName: "errorDownSpinBox"
                            Layout.preferredWidth: 54
                            from: 0
                            to: 16
                            editable: true
                            value: imageInput.errorDown
                            onValueModified: imageInput.errorDown = value
                            Accessible.name: qsTr("Error weight down")
                        }
                        Label { text: qsTr("↘") }
                        SpinBox {
                            objectName: "errorDownRightSpinBox"
                            Layout.preferredWidth: 54
                            from: 0
                            to: 16
                            editable: true
                            value: imageInput.errorDownRight
                            onValueModified: imageInput.errorDownRight = value
                            Accessible.name: qsTr("Error weight down-right")
                        }
                        Label { text: qsTr("→") }
                        SpinBox {
                            objectName: "errorRightSpinBox"
                            Layout.preferredWidth: 54
                            from: 0
                            to: 16
                            editable: true
                            value: imageInput.errorRight
                            onValueModified: imageInput.errorRight = value
                            Accessible.name: qsTr("Error weight right")
                        }
                        Label { text: qsTr("⇥") }
                        SpinBox {
                            objectName: "errorFarRightSpinBox"
                            Layout.preferredWidth: 54
                            from: 0
                            to: 16
                            editable: true
                            value: imageInput.errorFarRight
                            onValueModified: imageInput.errorFarRight = value
                            Accessible.name: qsTr("Error weight far-right")
                        }
                        Label { text: qsTr("⇓") }
                        SpinBox {
                            objectName: "errorDownTwoSpinBox"
                            Layout.preferredWidth: 54
                            from: 0
                            to: 16
                            editable: true
                            value: imageInput.errorDownTwo
                            onValueModified: imageInput.errorDownTwo = value
                            Accessible.name: qsTr("Error weight two rows down")
                        }
                    }
                    Label {
                        objectName: "errorWeightTotalLabel"
                        text: qsTr("Total: %1 / 16").arg(
                                  imageInput.errorDownLeft + imageInput.errorDown
                                  + imageInput.errorDownRight + imageInput.errorRight
                                  + imageInput.errorFarRight + imageInput.errorDownTwo)
                        color: palette.placeholderText
                        font.pixelSize: 11
                    }
                    Label { text: qsTr("Perceptual RGB weights (%)") }
                    GridLayout {
                        objectName: "perceptualWeightsGrid"
                        Layout.fillWidth: true
                        columns: 6
                        columnSpacing: 3
                        enabled: imageInput.perceptualColorMatching
                        Label { text: qsTr("R") }
                        SpinBox {
                            objectName: "perceptualRedWeightSpinBox"
                            Layout.preferredWidth: 58
                            from: 0; to: 100; editable: true
                            value: imageInput.perceptualRedWeight
                            onValueModified: imageInput.perceptualRedWeight = value
                            Accessible.name: qsTr("Perceptual red weight")
                        }
                        Label { text: qsTr("G") }
                        SpinBox {
                            objectName: "perceptualGreenWeightSpinBox"
                            Layout.preferredWidth: 58
                            from: 0; to: 100; editable: true
                            value: imageInput.perceptualGreenWeight
                            onValueModified: imageInput.perceptualGreenWeight = value
                            Accessible.name: qsTr("Perceptual green weight")
                        }
                        Label { text: qsTr("B") }
                        SpinBox {
                            objectName: "perceptualBlueWeightSpinBox"
                            Layout.preferredWidth: 58
                            from: 0; to: 100; editable: true
                            value: imageInput.perceptualBlueWeight
                            onValueModified: imageInput.perceptualBlueWeight = value
                            Accessible.name: qsTr("Perceptual blue weight")
                        }
                    }
                    Button {
                        objectName: "restorePerceptualWeightsButton"
                        text: qsTr("Restore 30 / 52 / 18")
                        Layout.fillWidth: true
                        onClicked: imageInput.restorePerceptualWeights()
                        Accessible.name: qsTr("Restore default perceptual weights")
                    }
                    Label { text: qsTr("Maximum color shift: %1%").arg(imageInput.maximumColorShift.toFixed(1)) }
                    Slider {
                        objectName: "maximumColorShiftSlider"
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        stepSize: 0.1
                        value: imageInput.maximumColorShift
                        onMoved: imageInput.maximumColorShift = value
                        activeFocusOnTab: true
                        Accessible.name: qsTr("Maximum color shift")
                    }
                    Label { text: qsTr("Gamma: %1").arg(imageInput.gamma.toFixed(2)) }
                    Slider {
                        Layout.fillWidth: true
                        from: 0.1
                        to: 3.0
                        stepSize: 0.05
                        value: imageInput.gamma
                        onMoved: imageInput.gamma = value
                        activeFocusOnTab: true
                        Accessible.name: qsTr("Gamma")
                    }
                    Label { text: qsTr("Luma emphasis: %1").arg(imageInput.lumaEmphasis.toFixed(2)) }
                    Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: 4
                        stepSize: 0.05
                        value: imageInput.lumaEmphasis
                        onMoved: imageInput.lumaEmphasis = value
                        activeFocusOnTab: true
                        Accessible.name: qsTr("Luma emphasis")
                    }
                    Label { text: qsTr("Flicker limit: %1%").arg(imageInput.maximumMulticolorDifference) }
                    Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        stepSize: 1
                        value: imageInput.maximumMulticolorDifference
                        onMoved: imageInput.maximumMulticolorDifference = Math.round(value)
                        activeFocusOnTab: true
                        Accessible.name: qsTr("Maximum multicolor flicker difference")
                    }
                    Label { text: qsTr("Ordered darkening: %1 / 16").arg(imageInput.orderedBrightness) }
                    Slider {
                        objectName: "orderedBrightnessSlider"
                        Layout.fillWidth: true
                        from: 0
                        to: 16
                        stepSize: 1
                        value: imageInput.orderedBrightness
                        onMoved: imageInput.orderedBrightness = Math.round(value)
                        activeFocusOnTab: true
                        Accessible.name: qsTr("Ordered dither darkening")
                    }
                    Label { text: qsTr("F18A palette selection") }
                    ComboBox {
                        objectName: "paletteSelectionCombo"
                        Layout.fillWidth: true
                        model: [qsTr("Median Cut"), qsTr("Popularity")]
                        currentIndex: imageInput.paletteSelectionMode
                        enabled: imageInput.conversionMode === 7
                                 || imageInput.conversionMode === 8
                        onActivated: imageInput.paletteSelectionMode = currentIndex
                        Accessible.name: qsTr("F18A palette selection algorithm")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        enabled: imageInput.conversionMode === 8
                        Label {
                            text: qsTr("Shared scanline colors")
                            Layout.fillWidth: true
                        }
                        SpinBox {
                            objectName: "scanlineStaticColorCountSpinBox"
                            from: 0
                            to: 14
                            editable: true
                            value: imageInput.scanlineStaticColorCount
                            onValueModified: imageInput.scanlineStaticColorCount = value
                            Accessible.name: qsTr("Shared scanline color count")
                        }
                    }
                    RowLayout {
                        objectName: "scanlineRegionsRow"
                        Layout.fillWidth: true
                        enabled: imageInput.conversionMode === 8
                                 && imageInput.scanlineStaticColorCount > 0
                        Label { text: qsTr("Regions") }
                        CheckBox {
                            objectName: "scanlineRegion1CheckBox"
                            text: qsTr("1")
                            checked: imageInput.scanlineRegion1
                            onToggled: imageInput.scanlineRegion1 = checked
                        }
                        CheckBox {
                            objectName: "scanlineRegion2CheckBox"
                            text: qsTr("2")
                            checked: imageInput.scanlineRegion2
                            onToggled: imageInput.scanlineRegion2 = checked
                        }
                        CheckBox {
                            objectName: "scanlineRegion3CheckBox"
                            text: qsTr("3")
                            checked: imageInput.scanlineRegion3
                            onToggled: imageInput.scanlineRegion3 = checked
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Undo")
                enabled: imageInput.canUndo
                Layout.fillWidth: true
                onClicked: imageInput.undoSettings()
                activeFocusOnTab: true
                Accessible.name: qsTr("Undo settings change")
            }
            Button {
                text: qsTr("Reset")
                Layout.fillWidth: true
                onClicked: imageInput.resetSettings()
                activeFocusOnTab: true
                Accessible.name: qsTr("Reset conversion settings")
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: imageInput.conversionMode <= 6
            spacing: 4

            ToolButton {
                id: workingPaletteToggle
                objectName: "workingPaletteSettingsToggle"
                text: checked ? qsTr("▾ Working palette") : qsTr("▸ Working palette")
                checkable: true
                Layout.fillWidth: true
                palette.buttonText: "#17324d"
                background: Rectangle {
                    objectName: "workingPaletteSettingsToggleBackground"
                    radius: 3
                    color: workingPaletteToggle.down ? "#78bce8"
                                                     : workingPaletteToggle.hovered ? "#a8d7f5"
                                                                                   : "#bfe3f8"
                    border.width: 1
                    border.color: "#69a9d1"
                }
                Accessible.name: qsTr("Show working palette settings")
                activeFocusOnTab: true
            }

            ThemedGroupBox {
                objectName: "workingPaletteSettingsGroup"
                visible: workingPaletteToggle.checked
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    GridLayout {
                        columns: 8
                        Repeater {
                            model: imageInput.workingPaletteColors
                            ToolButton {
                                required property var modelData
                                required property int index
                                implicitWidth: 28
                                implicitHeight: 28
                                Accessible.name: qsTr("Edit working palette color %1").arg(index + 1)
                                onClicked: {
                                    root.editedPaletteIndex = index
                                    paletteEditor.selectedColor = modelData
                                    paletteEditor.open()
                                }
                                background: Rectangle {
                                    color: modelData
                                    border.width: 1
                                    border.color: palette.text
                                    radius: 2
                                }
                            }
                        }
                    }
                    Button {
                        objectName: "resetWorkingPaletteButton"
                        text: qsTr("Restore default palette")
                        Layout.fillWidth: true
                        onClicked: imageInput.resetWorkingPalette()
                    }
                }
            }
        }

        ThemedGroupBox {
            objectName: "paletteSettingsGroup"
            title: qsTr("Screen Image palette")
            Layout.fillWidth: true
            visible: imageInput.paletteColors.length > 0

            ColumnLayout {
                anchors.fill: parent
                GridLayout {
                    columns: 8
                    Repeater {
                        model: imageInput.paletteColors
                        Rectangle {
                            required property var modelData
                            implicitWidth: 26
                            implicitHeight: 26
                            color: modelData
                            border.color: "#d8dde3"
                            Accessible.name: qsTr("Palette color %1").arg(modelData)
                        }
                    }
                }
                CheckBox {
                    id: scanlineToggle
                    visible: imageInput.scanlinePaletteAvailable
                    text: qsTr("Show scanline palette map")
                    activeFocusOnTab: true
                }
                Image {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    visible: scanlineToggle.visible && scanlineToggle.checked
                    source: imageInput.palettePreview
                    fillMode: Image.Stretch
                    smooth: false
                    Accessible.name: qsTr("Scanline palette visualization")
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            ToolButton {
                id: exportToggle
                objectName: "exportSettingsToggle"
                text: checked ? qsTr("▾ Export") : qsTr("▸ Export")
                checkable: true
                Layout.fillWidth: true
                palette.buttonText: "#17324d"
                background: Rectangle {
                    objectName: "exportSettingsToggleBackground"
                    radius: 3
                    color: exportToggle.down ? "#78bce8"
                                             : exportToggle.hovered ? "#a8d7f5"
                                                                    : "#bfe3f8"
                    border.width: 1
                    border.color: "#69a9d1"
                }
                Accessible.name: qsTr("Show export settings")
                activeFocusOnTab: true
            }

            ThemedGroupBox {
                objectName: "exportSettingsGroup"
                visible: exportToggle.checked
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    ComboBox {
                        Layout.fillWidth: true
                        model: [qsTr("TIFILES"), qsTr("V9T9"), qsTr("RAW tables"),
                                qsTr("RLE tables"), qsTr("MSX Screen 2"),
                                qsTr("Coleco CVPaint"), qsTr("Adam PowerPaint"),
                                qsTr("Adam HGR"), qsTr("PNG preview")]
                        currentIndex: imageInput.exportFormat
                        onActivated: imageInput.exportFormat = currentIndex
                        activeFocusOnTab: true
                        Accessible.name: qsTr("Export format")
                    }
                    Label {
                        Layout.fillWidth: true
                        text: imageInput.outputSummary
                        wrapMode: Text.WrapAnywhere
                        color: palette.placeholderText
                    }
                    Button {
                        text: qsTr("Choose folder and export…")
                        Layout.fillWidth: true
                        enabled: imageInput.hasConversion
                        onClicked: root.exportRequested()
                        activeFocusOnTab: true
                        Accessible.name: text
                    }
                }
            }
        }
    }
}

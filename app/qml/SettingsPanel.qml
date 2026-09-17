import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth
    signal exportRequested()

    ColumnLayout {
        width: root.availableWidth
        spacing: 12

        Label {
            text: qsTr("Conversion")
            font.pixelSize: 20
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }

        GroupBox {
            title: qsTr("Recommended starting point")
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

        GroupBox {
            title: qsTr("Common settings")
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
                    Layout.fillWidth: true
                    model: [
                        qsTr("None"),
                        qsTr("Floyd–Steinberg"),
                        qsTr("Atkinson"),
                        qsTr("Pattern"),
                        qsTr("Diagonal"),
                        qsTr("Ordered"),
                        qsTr("Ordered with error")
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
            }
        }

        GroupBox {
            title: qsTr("Crop and scale")
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
                Label {
                    text: qsTr("Horizontal position: %1").arg(imageInput.horizontalOffset)
                    visible: imageInput.fillMode !== 0
                }
                Slider {
                    Layout.fillWidth: true
                    visible: imageInput.fillMode !== 0
                    from: -256
                    to: 256
                    stepSize: 1
                    value: imageInput.horizontalOffset
                    onMoved: imageInput.horizontalOffset = Math.round(value)
                    Accessible.name: qsTr("Horizontal crop position")
                    activeFocusOnTab: true
                }
                Label {
                    text: qsTr("Vertical position: %1").arg(imageInput.verticalOffset)
                    visible: imageInput.fillMode !== 0
                }
                Slider {
                    Layout.fillWidth: true
                    visible: imageInput.fillMode !== 0
                    from: -192
                    to: 192
                    stepSize: 1
                    value: imageInput.verticalOffset
                    onMoved: imageInput.verticalOffset = Math.round(value)
                    Accessible.name: qsTr("Vertical crop position")
                    activeFocusOnTab: true
                }
            }
        }

        ToolButton {
            id: advancedToggle
            text: checked ? qsTr("▾ Advanced settings") : qsTr("▸ Advanced settings")
            checkable: true
            Layout.fillWidth: true
            Accessible.name: qsTr("Show advanced conversion settings")
            activeFocusOnTab: true
        }

        GroupBox {
            title: qsTr("Advanced")
            visible: advancedToggle.checked
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                Label { text: qsTr("Maximum color shift: %1%").arg(imageInput.maximumColorShift.toFixed(1)) }
                Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 20
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
                Label { text: qsTr("Ordered brightness: %1").arg(imageInput.orderedBrightness) }
                Slider {
                    Layout.fillWidth: true
                    from: -16
                    to: 16
                    stepSize: 1
                    value: imageInput.orderedBrightness
                    onMoved: imageInput.orderedBrightness = Math.round(value)
                    activeFocusOnTab: true
                    Accessible.name: qsTr("Ordered dither brightness")
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

        GroupBox {
            title: qsTr("Palette")
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

        GroupBox {
            title: qsTr("Export")
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

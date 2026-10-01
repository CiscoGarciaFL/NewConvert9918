import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    title: qsTr("Extract Patterns from Screen Image")
    modal: true
    width: 700
    height: 720

    readonly property int cellWidth: editorProject.characterPatternWidth
    readonly property int cellHeight: editorProject.characterPatternHeight
    readonly property int screenColumns: editorProject.characterMapColumns
    readonly property int screenRows: editorProject.characterMapRows
    readonly property int patternCount: regionWidthSpin.value * regionHeightSpin.value
    readonly property bool destinationFits:
        destinationPatternSpin.value + patternCount
        <= editorProject.characterPatternsPerSet

    function openForScreenImage() {
        destinationPatternSpin.value = editorProject.activeCharacterPattern
        if (editorProject.screenImageSelectionCharacterAligned) {
            regionXSpin.value = imageInput.screenImageSelectionX / cellWidth
            regionYSpin.value = imageInput.screenImageSelectionY / cellHeight
            regionWidthSpin.value = imageInput.screenImageSelectionWidth / cellWidth
            regionHeightSpin.value = imageInput.screenImageSelectionHeight / cellHeight
        } else {
            regionXSpin.value = 0
            regionYSpin.value = 0
            regionWidthSpin.value = 1
            regionHeightSpin.value = 1
        }
        open()
    }

    onAccepted: editorProject.extractScreenImagePatterns(
        regionXSpin.value, regionYSpin.value,
        regionWidthSpin.value, regionHeightSpin.value,
        destinationPatternSpin.value, layoutCombo.currentIndex === 1)

    footer: DialogButtonBox {
        Button {
            objectName: "characterExtractionCommitButton"
            text: qsTr("Extract")
            enabled: imageInput.hasConversion && root.destinationFits
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: qsTr("The active target uses %1×%2-pixel patterns, %3 patterns per set, and a %4×%5 character screen.")
                      .arg(root.cellWidth).arg(root.cellHeight)
                      .arg(editorProject.characterPatternsPerSet)
                      .arg(root.screenColumns).arg(root.screenRows)
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: editorProject.screenImageSelectionCharacterAligned
            text: qsTr("The character-aligned Screen Image selection initialized this region.")
            color: palette.highlight
            wrapMode: Text.WordWrap
        }

        Rectangle {
            id: screenPreviewFrame
            objectName: "characterExtractionScreenPreview"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 512
            Layout.preferredHeight: 384
            Layout.maximumWidth: root.width - 40
            color: "#10161c"
            border.width: 1
            border.color: "#66727e"
            clip: true

            Image {
                anchors.fill: parent
                source: imageInput.convertedPreview
                fillMode: Image.Stretch
                smooth: false
            }

            Canvas {
                id: regionOverlay
                anchors.fill: parent
                property int regionX: regionXSpin.value
                property int regionY: regionYSpin.value
                property int regionWidth: regionWidthSpin.value
                property int regionHeight: regionHeightSpin.value
                onRegionXChanged: requestPaint()
                onRegionYChanged: requestPaint()
                onRegionWidthChanged: requestPaint()
                onRegionHeightChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const context = getContext("2d")
                    context.clearRect(0, 0, width, height)
                    context.lineWidth = 1
                    context.strokeStyle = "#55ffffff"
                    context.beginPath()
                    for (let column = 1; column < root.screenColumns; ++column) {
                        const x = Math.round(column * width / root.screenColumns) + 0.5
                        context.moveTo(x, 0)
                        context.lineTo(x, height)
                    }
                    for (let row = 1; row < root.screenRows; ++row) {
                        const y = Math.round(row * height / root.screenRows) + 0.5
                        context.moveTo(0, y)
                        context.lineTo(width, y)
                    }
                    context.stroke()
                    context.fillStyle = "#3038a8ff"
                    context.strokeStyle = "#ffffff"
                    context.lineWidth = 2
                    const x = regionX * width / root.screenColumns
                    const y = regionY * height / root.screenRows
                    const selectionWidth = regionWidth * width / root.screenColumns
                    const selectionHeight = regionHeight * height / root.screenRows
                    context.fillRect(x, y, selectionWidth, selectionHeight)
                    context.strokeRect(x + 1, y + 1,
                                       Math.max(1, selectionWidth - 2),
                                       Math.max(1, selectionHeight - 2))
                }
            }

            MouseArea {
                anchors.fill: parent
                property int anchorColumn: 0
                property int anchorRow: 0
                function cellColumn(localX) {
                    return Math.max(0, Math.min(root.screenColumns - 1,
                        Math.floor(localX * root.screenColumns / Math.max(1, width))))
                }
                function cellRow(localY) {
                    return Math.max(0, Math.min(root.screenRows - 1,
                        Math.floor(localY * root.screenRows / Math.max(1, height))))
                }
                function updateRegion(localX, localY) {
                    const column = cellColumn(localX)
                    const row = cellRow(localY)
                    regionXSpin.value = Math.min(anchorColumn, column)
                    regionYSpin.value = Math.min(anchorRow, row)
                    regionWidthSpin.value = Math.abs(column - anchorColumn) + 1
                    regionHeightSpin.value = Math.abs(row - anchorRow) + 1
                }
                onPressed: mouse => {
                    anchorColumn = cellColumn(mouse.x)
                    anchorRow = cellRow(mouse.y)
                    updateRegion(mouse.x, mouse.y)
                }
                onPositionChanged: mouse => {
                    if (pressed)
                        updateRegion(mouse.x, mouse.y)
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 8
            rowSpacing: 6

            Label { text: qsTr("Region X") }
            SpinBox {
                id: regionXSpin
                objectName: "characterExtractionRegionX"
                from: 0
                to: Math.max(0, root.screenColumns - 1)
                editable: true
            }
            Label { text: qsTr("Region Y") }
            SpinBox {
                id: regionYSpin
                objectName: "characterExtractionRegionY"
                from: 0
                to: Math.max(0, root.screenRows - 1)
                editable: true
            }
            Label { text: qsTr("Pattern width") }
            SpinBox {
                id: regionWidthSpin
                objectName: "characterExtractionRegionWidth"
                from: 1
                to: Math.max(1, root.screenColumns - regionXSpin.value)
                value: 1
                editable: true
            }
            Label { text: qsTr("Pattern height") }
            SpinBox {
                id: regionHeightSpin
                objectName: "characterExtractionRegionHeight"
                from: 1
                to: Math.max(1, root.screenRows - regionYSpin.value)
                value: 1
                editable: true
            }
            Label { text: qsTr("Destination pattern") }
            SpinBox {
                id: destinationPatternSpin
                objectName: "characterExtractionDestinationPattern"
                from: 0
                to: Math.max(0, editorProject.characterPatternsPerSet - 1)
                editable: true
            }
            Label { text: qsTr("Pattern order") }
            ComboBox {
                id: layoutCombo
                objectName: "characterExtractionLayout"
                model: [qsTr("Horizontal wrap"), qsTr("Vertical wrap")]
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.destinationFits
                  ? qsTr("%1 patterns will be written to active Set %2, starting at pattern %3. The extraction is one undoable edit.")
                        .arg(root.patternCount)
                        .arg(editorProject.activeCharacterSet + 1)
                        .arg(destinationPatternSpin.value)
                  : qsTr("The %1-pattern region does not fit in this set from pattern %2.")
                        .arg(root.patternCount).arg(destinationPatternSpin.value)
            color: root.destinationFits ? palette.placeholderText : "#e87878"
            wrapMode: Text.WordWrap
        }
    }
}

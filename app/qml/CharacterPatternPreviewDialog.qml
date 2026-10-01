import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    title: qsTr("Pattern Previewer")
    modal: true
    width: 700
    height: 620
    standardButtons: Dialog.Close

    function openPreview() {
        firstPatternSpin.value = editorProject.activeCharacterPattern
        open()
    }

    ColumnLayout {
        width: parent.width
        height: parent.height
        spacing: 10

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 8
            rowSpacing: 6

            Label { text: qsTr("First pattern") }
            SpinBox {
                id: firstPatternSpin
                objectName: "patternPreviewFirstPattern"
                from: 0
                to: Math.max(0, editorProject.characterPatternsPerSet - 1)
                editable: true
            }
            Label { text: qsTr("Set") }
            ComboBox {
                id: setCombo
                objectName: "patternPreviewSet"
                model: editorProject.characterSetNames
                currentIndex: editorProject.activeCharacterSet
            }
            Label { text: qsTr("Pattern width") }
            SpinBox {
                id: previewWidthSpin
                objectName: "patternPreviewWidth"
                from: 1
                to: editorProject.characterMapColumns
                value: 4
                editable: true
            }
            Label { text: qsTr("Pattern height") }
            SpinBox {
                id: previewHeightSpin
                objectName: "patternPreviewHeight"
                from: 1
                to: editorProject.characterMapRows
                value: 4
                editable: true
            }
            Label { text: qsTr("Pattern order") }
            ComboBox {
                id: previewLayoutCombo
                objectName: "patternPreviewLayout"
                model: [qsTr("Horizontal wrap"), qsTr("Vertical wrap")]
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Previewing %1×%2 patterns from Set %3. Change the width, height, starting pattern, or wrapping direction to inspect the region.")
                      .arg(previewWidthSpin.value).arg(previewHeightSpin.value)
                      .arg(setCombo.currentIndex + 1)
            wrapMode: Text.WordWrap
            color: palette.placeholderText
        }

        Rectangle {
            objectName: "patternPreviewCanvas"
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#10161c"
            border.width: 1
            border.color: "#66727e"
            clip: true

            Image {
                anchors.fill: parent
                anchors.margins: 10
                source: editorProject.characterPatternPreview(
                    setCombo.currentIndex, firstPatternSpin.value,
                    previewWidthSpin.value, previewHeightSpin.value,
                    previewLayoutCombo.currentIndex === 1)
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: false
            }
        }
    }
}

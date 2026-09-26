pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property int drawingTool
    required property real placementScale

    function paletteColor(index) {
        const colors = editorProject.characterPaletteColors
        if (index < 0 || index >= colors.length)
            return "#000000"
        return index === 0 ? "#20272e" : colors[index]
    }

    component SpriteBank: Item {
        id: bank
        required property int spriteSize
        required property string title

        ColumnLayout {
            anchors.fill: parent
            spacing: 3

            Label {
                Layout.fillWidth: true
                text: bank.title
                font.weight: Font.DemiBold
                color: palette.placeholderText
            }

            GridView {
                id: spriteGrid
                objectName: bank.spriteSize === 8
                            ? "sprite8PatternGrid" : "sprite16PatternGrid"
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: 32
                clip: true
                cellWidth: Math.max(48, Math.floor(width / 8))
                cellHeight: Math.max(48, cellWidth)
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: spriteCell
                    required property int index
                    readonly property bool active:
                        index === editorProject.activeSprite
                        && bank.spriteSize === editorProject.activeSpriteSize
                    readonly property var pixels: {
                        const revision = editorProject.spriteRevision
                        return editorProject.spritePatternPixels(
                            editorProject.activeSpriteSet, index, bank.spriteSize)
                    }
                    width: spriteGrid.cellWidth - 3
                    height: spriteGrid.cellHeight - 3
                    color: active ? "#315f82" : "#1b2229"
                    border.width: active ? 2 : 1
                    border.color: active ? "#8fd0ff" : "#53606d"
                    radius: 2

                    Item {
                        id: thumbnail
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 8, parent.height - 8)
                        height: width

                        Repeater {
                            model: bank.spriteSize * bank.spriteSize
                            Rectangle {
                                required property int index
                                readonly property int value:
                                    spriteCell.pixels.length > index
                                    ? spriteCell.pixels[index] : 0
                                x: (index % bank.spriteSize)
                                   * thumbnail.width / bank.spriteSize
                                y: Math.floor(index / bank.spriteSize)
                                   * thumbnail.height / bank.spriteSize
                                width: Math.ceil(thumbnail.width / bank.spriteSize)
                                height: Math.ceil(thumbnail.height / bank.spriteSize)
                                color: root.paletteColor(value)
                            }
                        }
                    }

                    Label {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: 2
                        padding: 1
                        text: spriteCell.index.toString(16).toUpperCase()
                                         .padStart(2, "0")
                        color: "white"
                        font.pixelSize: 10
                        background: Rectangle { color: "#99000000"; radius: 1 }
                    }

                    TapHandler {
                        onTapped: {
                            editorProject.activeSprite = spriteCell.index
                            editorProject.activeSpriteSize = bank.spriteSize
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        Rectangle {
            id: editorTray
            objectName: "spritePatternEditorTray"
            Layout.fillWidth: true
            Layout.preferredHeight: editorProject.spritePlacementMode
                                    ? 192 * root.placementScale + 18 : 204
            Layout.minimumHeight: editorProject.spritePlacementMode ? 210 : 204
            color: "#0f151a"
            border.width: 1
            border.color: "#46515d"
            radius: 4

            SpritePatternEditor {
                objectName: "spritePatternEditor"
                anchors.centerIn: parent
                visible: !editorProject.spritePlacementMode
                setIndex: editorProject.activeSpriteSet
                spriteIndex: editorProject.activeSprite
                spriteSize: editorProject.activeSpriteSize
                drawingTool: root.drawingTool
                active: true
                onSelected: {}
            }

            SpritePlacementView {
                objectName: "spritePlacementWorkspace"
                anchors.fill: parent
                anchors.margins: 4
                visible: editorProject.spritePlacementMode
                enabled: visible
                zoomScale: root.placementScale
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: editorProject.editScope === 1
                      ? qsTr("F18A: each sprite may select its own bank")
                      : qsTr("TMS9918A: the global size selects one bank")
                color: palette.placeholderText
                font.pixelSize: 11
            }
            Label { text: qsTr("Set") }
            ComboBox {
                objectName: "spriteSetComboBox"
                Layout.preferredWidth: 76
                model: editorProject.spriteSetNames
                currentIndex: editorProject.activeSpriteSet
                onActivated: index => editorProject.activeSpriteSet = index
            }
            Label { text: qsTr("Sprite") }
            SpinBox {
                objectName: "spriteItemSpinBox"
                implicitWidth: 60
                from: 0
                to: 31
                value: editorProject.activeSprite
                editable: true
                onValueModified: editorProject.activeSprite = value
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            SpriteBank {
                objectName: "spritePatternGrid"
                SplitView.preferredHeight: parent.height / 2
                SplitView.minimumHeight: 80
                spriteSize: 8
                title: qsTr("8×8 sprite patterns · 32")
            }
            SpriteBank {
                SplitView.preferredHeight: parent.height / 2
                SplitView.minimumHeight: 80
                spriteSize: 16
                title: qsTr("16×16 sprite patterns · 32")
            }
        }
    }
}

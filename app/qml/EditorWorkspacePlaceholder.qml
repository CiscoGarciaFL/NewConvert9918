pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Character and sprite editors use the same workspace chrome as Screen Image.
// Their eventual canvases and tools can be added without allowing the three
// destination layouts to drift apart.
PreviewPane {
    id: root

    required property string description
    required property string importDescription
    required property int editorKind // 0 = character, 1 = sprite

    imageSource: ""
    details: editorKind === 0
             ? qsTr("Set %1 of 3 · Pattern %2 of 256")
                   .arg(editorProject.activeCharacterSet + 1)
                   .arg(editorProject.activeCharacterPattern + 1)
             : qsTr("Sprite set %1 of %2 · Sprite %3 of 32")
                   .arg(editorProject.activeSpriteSet + 1)
                   .arg(editorProject.spriteSetNames.length)
                   .arg(editorProject.activeSprite + 1)
    emptyText: root.description + "\n\n" + root.importDescription

    upperToolbarContent: Component {
        RowLayout {
            spacing: 2

            ToolButton {
                objectName: root.objectName + "ImportSourceButton"
                implicitHeight: 26
                text: qsTr("↳")
                enabled: false
                Accessible.name: qsTr("Import from Source")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Source-to-pattern extraction will connect here next")
            }
            ToolSeparator { height: 26 }
            ToolButton {
                objectName: root.objectName + "SetGridButton"
                implicitHeight: 26
                text: qsTr("▦")
                checkable: root.editorKind === 1
                checked: root.editorKind === 0 || !editorProject.spritePlacementMode
                Accessible.name: root.editorKind === 0
                                 ? qsTr("Character pattern set")
                                 : qsTr("Sprite set grid")
                onClicked: {
                    if (root.editorKind === 1)
                        editorProject.spritePlacementMode = false
                }
            }
            ToolButton {
                objectName: root.objectName + "PlacementButton"
                visible: root.editorKind === 1
                implicitHeight: 26
                text: qsTr("⌗")
                checkable: true
                checked: editorProject.spritePlacementMode
                Accessible.name: qsTr("Sprite placement workspace")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Arrange a 32-sprite set over the Screen Image reference")
                onClicked: editorProject.spritePlacementMode = true
            }
            Item { Layout.fillWidth: true }
            Label {
                text: editorProject.editScope === 1
                      ? qsTr("Editing F18A enhancements")
                      : qsTr("Editing shared 9918A baseline")
                color: palette.placeholderText
            }
        }
    }

    workspaceContent: Component {
        Item {
            CharacterSetView {
                anchors.fill: parent
                visible: root.editorKind === 0
            }
            SpriteSetView {
                anchors.fill: parent
                visible: root.editorKind === 1
            }
        }
    }

    lowerToolbarContent: Component {
        RowLayout {
            spacing: 2

            Label { text: root.editorKind === 0 ? qsTr("Set") : qsTr("Sprite set") }
            ComboBox {
                objectName: root.objectName + "SetComboBox"
                Layout.preferredWidth: 110
                model: root.editorKind === 0
                       ? editorProject.characterSetNames
                       : editorProject.spriteSetNames
                currentIndex: root.editorKind === 0
                              ? editorProject.activeCharacterSet
                              : editorProject.activeSpriteSet
                onActivated: index => {
                    if (root.editorKind === 0)
                        editorProject.activeCharacterSet = index
                    else
                        editorProject.activeSpriteSet = index
                }
            }
            ToolButton {
                objectName: root.objectName + "AddSetButton"
                visible: root.editorKind === 1
                implicitWidth: 26
                implicitHeight: 26
                text: qsTr("+")
                Accessible.name: qsTr("Add sprite set")
                onClicked: editorProject.addSpriteSet()
            }
            ToolButton {
                objectName: root.objectName + "RemoveSetButton"
                visible: root.editorKind === 1
                implicitWidth: 26
                implicitHeight: 26
                text: qsTr("−")
                enabled: editorProject.spriteSetNames.length > 1
                Accessible.name: qsTr("Remove active sprite set")
                onClicked: editorProject.removeActiveSpriteSet()
            }
            ToolSeparator { height: 26 }
            Label { text: root.editorKind === 0 ? qsTr("Pattern") : qsTr("Sprite") }
            SpinBox {
                objectName: root.objectName + "ItemSpinBox"
                implicitWidth: 72
                from: 0
                to: root.editorKind === 0 ? 255 : 31
                value: root.editorKind === 0
                       ? editorProject.activeCharacterPattern
                       : editorProject.activeSprite
                editable: true
                onValueModified: {
                    if (root.editorKind === 0)
                        editorProject.activeCharacterPattern = value
                    else
                        editorProject.activeSprite = value
                }
            }
            Item { Layout.fillWidth: true }
        }
    }
}

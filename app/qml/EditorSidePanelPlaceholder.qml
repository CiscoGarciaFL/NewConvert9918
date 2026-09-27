import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    required property string editorTitle
    required property string importDescription
    required property int editorKind // 0 = character, 1 = sprite
    property bool closable: false
    signal closeRequested()
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: root.availableWidth
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: root.editorTitle
                font.pixelSize: 20
                font.weight: Font.DemiBold
                Layout.fillWidth: true
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
            objectName: root.objectName + "OutputProfilesGroup"
            title: qsTr("Output Profiles")
            Layout.fillWidth: true

            OutputProfileControls {
                anchors.fill: parent
                objectPrefix: root.objectName + "OutputProfiles"
            }
        }

        ThemedGroupBox {
            objectName: root.objectName + "EditScopeGroup"
            title: qsTr("Editing Scope")
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                RadioButton {
                    text: qsTr("Shared TMS9918A baseline")
                    checked: editorProject.editScope === 0
                    onClicked: editorProject.editScope = 0
                }
                RadioButton {
                    text: qsTr("F18A enhancements")
                    enabled: editorProject.f18aEnabled
                    checked: editorProject.editScope === 1
                    onClicked: editorProject.editScope = 1
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Baseline edits feed both outputs. F18A-only edits are stored as non-destructive overrides.")
                    wrapMode: Text.WordWrap
                    color: palette.placeholderText
                }
            }
        }

        ThemedGroupBox {
            title: qsTr("Import from Source")
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                Label {
                    Layout.fillWidth: true
                    text: root.importDescription
                    wrapMode: Text.WordWrap
                    color: palette.placeholderText
                }
                Button {
                    objectName: root.objectName + "ImportFromSourceButton"
                    Layout.fillWidth: true
                    text: root.editorKind === 0
                          ? qsTr("Load Screen Image into pattern sets")
                          : qsTr("Populate sprite placement from Screen Image")
                    enabled: false
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("The repeatable recipe and set destination are ready; pixel extraction is the next implementation layer")
                }
            }
        }

        ThemedGroupBox {
            objectName: root.objectName + "SetOptionsGroup"
            title: root.editorKind === 0 ? qsTr("Pattern Sets") : qsTr("Sprite Sets")
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                Label {
                    Layout.fillWidth: true
                    text: root.editorKind === 0
                          ? qsTr("Three simultaneously available sets, each containing 256 pattern slots. Active: Set %1, pattern %2.")
                                .arg(editorProject.activeCharacterSet + 1)
                                .arg(editorProject.activeCharacterPattern)
                          : qsTr("Each set contains 32 8×8 patterns, 32 16×16 patterns, and 32 independently placed sprites. Active: Set %1, sprite %2.")
                                .arg(editorProject.activeSpriteSet + 1)
                                .arg(editorProject.activeSprite)
                    wrapMode: Text.WordWrap
                }
                GridLayout {
                    visible: root.editorKind === 1
                    columns: 2
                    Layout.fillWidth: true
                    Label { text: qsTr("9918A global size") }
                    ComboBox {
                        objectName: root.objectName + "GlobalSpriteSizeComboBox"
                        model: [qsTr("8×8"), qsTr("16×16")]
                        currentIndex: editorProject.spriteGlobalSize === 16 ? 1 : 0
                        onActivated: index =>
                            editorProject.spriteGlobalSize = index === 1 ? 16 : 8
                    }
                    Label { text: qsTr("Active F18A size") }
                    ComboBox {
                        objectName: root.objectName + "ActiveSpriteSizeComboBox"
                        model: [qsTr("8×8"), qsTr("16×16")]
                        enabled: editorProject.editScope === 1
                        currentIndex: editorProject.activeSpriteSize === 16 ? 1 : 0
                        onActivated: index =>
                            editorProject.activeSpriteSize = index === 1 ? 16 : 8
                    }
                    Label { text: qsTr("F18A color depth") }
                    ComboBox {
                        objectName: root.objectName + "SpriteColorDepthComboBox"
                        model: [qsTr("1 bpp · 2 indexes"),
                                qsTr("2 bpp · 4 indexes"),
                                qsTr("3 bpp · 8 indexes")]
                        enabled: editorProject.editScope === 1
                        currentIndex: editorProject.activeSpriteColorDepth - 1
                        onActivated: index =>
                            editorProject.activeSpriteColorDepth = index + 1
                    }
                    Label { text: qsTr("Placement width") }
                    SpinBox {
                        objectName: root.objectName + "PlacementWidthSpinBox"
                        from: 8
                        to: 1024
                        value: editorProject.placementWidth
                        editable: true
                        onValueModified: editorProject.placementWidth = value
                    }
                    Label { text: qsTr("Placement height") }
                    SpinBox {
                        objectName: root.objectName + "PlacementHeightSpinBox"
                        from: 8
                        to: 1024
                        value: editorProject.placementHeight
                        editable: true
                        onValueModified: editorProject.placementHeight = value
                    }
                }
                Label {
                    visible: root.editorKind === 1
                    Layout.fillWidth: true
                    text: editorProject.editScope === 0
                          ? qsTr("The TMS9918A uses one global sprite size and one visible color per sprite. Both pattern banks remain editable, but the selected global bank is used for placement and export.")
                          : qsTr("F18A enhancements may choose 8×8 or 16×16 and 1/2/3-bpp color independently for each sprite. Unchanged pixels continue to inherit the shared baseline.")
                    wrapMode: Text.WordWrap
                    color: palette.placeholderText
                }
            }
        }
    }
}

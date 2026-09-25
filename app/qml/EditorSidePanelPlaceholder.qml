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
                          : qsTr("Each set contains 32 sprite slots. Add sets for animation frames or alternate images. Active: Set %1, sprite %2.")
                                .arg(editorProject.activeSpriteSet + 1)
                                .arg(editorProject.activeSprite)
                    wrapMode: Text.WordWrap
                }
                GridLayout {
                    visible: root.editorKind === 1
                    columns: 2
                    Layout.fillWidth: true
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
            }
        }
    }
}

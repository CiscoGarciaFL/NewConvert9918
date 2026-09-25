import QtQuick

Item {
    id: root

    required property int workspaceMode
    property bool closable: false
    signal exportRequested()
    signal closeRequested()

    Loader {
        anchors.fill: parent
        sourceComponent: root.workspaceMode === 1
                         ? characterSidePanel
                         : root.workspaceMode === 2
                           ? spriteSidePanel
                           : screenImageSidePanel
    }

    Component {
        id: screenImageSidePanel
        SettingsPanel {
            objectName: "screenImageSidePanel"
            closable: root.closable
            onExportRequested: root.exportRequested()
            onCloseRequested: root.closeRequested()
        }
    }

    Component {
        id: characterSidePanel
        EditorSidePanelPlaceholder {
            objectName: "characterEditorSidePanel"
            editorTitle: qsTr("Character Options")
            editorKind: 0
            importDescription: qsTr("Character import will select a source region, choose target character dimensions, scale it, reduce its colors, and fit it to the active hardware palette before committing editable pattern data.")
            closable: root.closable
            onCloseRequested: root.closeRequested()
        }
    }

    Component {
        id: spriteSidePanel
        EditorSidePanelPlaceholder {
            objectName: "spriteEditorSidePanel"
            editorTitle: qsTr("Sprite Options")
            editorKind: 1
            importDescription: qsTr("Sprite import will select a source region, choose sprite dimensions and transparency, scale it, reduce its colors, and fit it to the active hardware palette before committing editable sprite data.")
            closable: root.closable
            onCloseRequested: root.closeRequested()
        }
    }
}

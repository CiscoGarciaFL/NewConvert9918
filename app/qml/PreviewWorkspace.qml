import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // 0 = tabbed, 1 = horizontal split, 2 = vertical split.
    property int layoutMode: 1
    // 0 = Screen Image, 1 = Character Editor, 2 = Sprite Editor.
    property int workspaceMode: 0
    readonly property string destinationTitle: workspaceMode === 1
                                               ? qsTr("Character Editor")
                                               : workspaceMode === 2
                                                 ? qsTr("Sprite Editor")
                                                 : qsTr("Screen Image")

    component SourcePane: PreviewPane {
        objectName: "sourcePreview"
        title: qsTr("Source")
        imageSource: imageInput.sourcePreview
        details: imageInput.hasImage ? imageInput.sourceDetails : ""
        emptyText: qsTr("Drop an image here or choose Open")
        acceptDrops: true
        sourceTools: true
        onFileDropped: fileUrl => imageInput.openUrl(fileUrl)
        onColorPointPicked: (normalizedX, normalizedY, foreground) =>
            imageInput.pickColor(normalizedX, normalizedY, foreground)
    }

    component ScreenImagePane: PreviewPane {
        objectName: "convertedPreview"
        title: qsTr("Screen Image")
        imageSource: imageInput.convertedPreview
        details: imageInput.conversionDetails
        emptyText: imageInput.hasImage
                   ? qsTr("The screen image will appear here")
                   : qsTr("Open a source image to begin")
        busy: imageInput.busy
        screenImageTools: true
        convertedTools: true
        showTitle: root.layoutMode !== 0
        onColorPointPicked: (normalizedX, normalizedY, foreground) =>
            imageInput.pickScreenImageColor(normalizedX, normalizedY, foreground)
    }

    component CharacterEditorPane: EditorWorkspacePlaceholder {
        objectName: "characterEditorWorkspace"
        title: qsTr("Character Editor")
        description: qsTr("Create and edit character patterns under the active target VDP's rules.")
        importDescription: qsTr("Source-image selection, scaling, color reduction, and palette fitting will feed this editor.")
        editorKind: 0
        showTitle: root.layoutMode !== 0
    }

    component SpriteEditorPane: EditorWorkspacePlaceholder {
        objectName: "spriteEditorWorkspace"
        title: qsTr("Sprite Editor")
        description: qsTr("Create and edit sprite patterns under the active target VDP's rules.")
        importDescription: qsTr("Source-image selection, scaling, transparency, color reduction, and palette fitting will feed this editor.")
        editorKind: 1
        showTitle: root.layoutMode !== 0
    }

    component DestinationPane: Loader {
        sourceComponent: root.workspaceMode === 1
                         ? characterEditorPane
                         : root.workspaceMode === 2
                           ? spriteEditorPane
                           : screenImagePane
    }

    Component {
        id: screenImagePane
        ScreenImagePane {}
    }

    Component {
        id: characterEditorPane
        CharacterEditorPane {}
    }

    Component {
        id: spriteEditorPane
        SpriteEditorPane {}
    }

    Loader {
        anchors.fill: parent
        sourceComponent: root.layoutMode === 0 ? tabbedLayout : splitLayout
    }

    Component {
        id: tabbedLayout

        ColumnLayout {
            objectName: "tabbedPreviewLayout"
            spacing: 8

            TabBar {
                id: previewTabs
                objectName: "previewTabBar"
                Layout.fillWidth: true

                TabButton {
                    objectName: "sourcePreviewTab"
                    text: qsTr("Source")
                    Accessible.name: qsTr("Show source image")
                }
                TabButton {
                    objectName: "convertedPreviewTab"
                    text: root.destinationTitle
                    Accessible.name: qsTr("Show %1").arg(root.destinationTitle)
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: previewTabs.currentIndex

                SourcePane {
                    showTitle: false
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                DestinationPane {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    }

    Component {
        id: splitLayout

        SplitView {
            id: splitPreviewLayout
            readonly property real handleThickness: 2
            objectName: root.layoutMode === 1
                        ? "horizontalPreviewLayout" : "verticalPreviewLayout"
            orientation: root.layoutMode === 1 ? Qt.Horizontal : Qt.Vertical

            SourcePane {
                SplitView.preferredWidth: (splitPreviewLayout.width
                                           - splitPreviewLayout.handleThickness) / 2
                SplitView.preferredHeight: (splitPreviewLayout.height
                                            - splitPreviewLayout.handleThickness) / 2
                SplitView.minimumWidth: 220
                SplitView.minimumHeight: 180
            }

            DestinationPane {
                SplitView.preferredWidth: (splitPreviewLayout.width
                                           - splitPreviewLayout.handleThickness) / 2
                SplitView.preferredHeight: (splitPreviewLayout.height
                                            - splitPreviewLayout.handleThickness) / 2
                SplitView.minimumWidth: 220
                SplitView.minimumHeight: 180
            }
        }
    }
}

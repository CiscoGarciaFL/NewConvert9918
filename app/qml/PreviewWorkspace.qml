import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // 0 = tabbed, 1 = horizontal split, 2 = vertical split.
    property int layoutMode: 1

    component SourcePane: PreviewPane {
        objectName: "sourcePreview"
        title: qsTr("Source")
        imageSource: imageInput.sourcePreview
        details: imageInput.hasImage
                 ? imageInput.sourceName + "\n" + imageInput.sourceDetails : ""
        emptyText: qsTr("Drop an image here or choose Open")
        acceptDrops: true
        cropOverlay: imageInput.hasImage && imageInput.fillMode !== 0
        onFileDropped: fileUrl => imageInput.openUrl(fileUrl)
    }

    component ConvertedPane: PreviewPane {
        objectName: "convertedPreview"
        title: qsTr("Converted")
        imageSource: imageInput.convertedPreview
        details: imageInput.conversionDetails
        emptyText: imageInput.hasImage
                   ? qsTr("The converted preview will appear here")
                   : qsTr("Open a source image to begin")
        busy: imageInput.busy
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
                    text: qsTr("Converted")
                    Accessible.name: qsTr("Show converted preview")
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
                ConvertedPane {
                    showTitle: false
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    }

    Component {
        id: splitLayout

        SplitView {
            objectName: root.layoutMode === 1
                        ? "horizontalPreviewLayout" : "verticalPreviewLayout"
            orientation: root.layoutMode === 1 ? Qt.Horizontal : Qt.Vertical

            SourcePane {
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                SplitView.minimumWidth: 220
                SplitView.minimumHeight: 180
            }

            ConvertedPane {
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                SplitView.minimumWidth: 220
                SplitView.minimumHeight: 180
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property string title
    property url imageSource
    property string emptyText: qsTr("No image")
    property string details
    property bool busy: false
    property bool cropOverlay: false
    property bool acceptDrops: false
    property bool showTitle: true
    signal fileDropped(url fileUrl)

    property bool fitToView: true
    property real manualZoom: 1.0
    readonly property real fitZoom: {
        if (preview.sourceSize.width <= 0 || preview.sourceSize.height <= 0)
            return 1.0
        return Math.min(viewport.width / preview.sourceSize.width,
                        viewport.height / preview.sourceSize.height)
    }
    readonly property real effectiveZoom: fitToView ? fitZoom : manualZoom

    function zoomBy(factor) {
        fitToView = false
        manualZoom = Math.max(0.125, Math.min(16, manualZoom * factor))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            objectName: root.objectName + "Title"
            Layout.fillWidth: true
            visible: root.showTitle
            text: root.title
            font.weight: Font.DemiBold
        }

        Rectangle {
            id: viewport
            objectName: root.objectName + "Viewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 180
            color: "#101317"
            border.color: "#53606d"
            radius: 4
            clip: true

            Flickable {
                id: flick
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: Math.max(width, preview.width)
                contentHeight: Math.max(height, preview.height)

                Image {
                    id: preview
                    objectName: root.objectName + "Image"
                    source: root.imageSource
                    asynchronous: true
                    cache: false
                    width: Math.max(1, sourceSize.width * root.effectiveZoom)
                    height: Math.max(1, sourceSize.height * root.effectiveZoom)
                    x: Math.max(0, (flick.width - width) / 2)
                    y: Math.max(0, (flick.height - height) / 2)
                    fillMode: Image.Stretch
                    smooth: root.effectiveZoom < 1
                }

                Rectangle {
                    visible: root.cropOverlay && preview.status === Image.Ready
                    x: preview.x + Math.max(8, preview.width * 0.08)
                    y: preview.y + Math.max(8, preview.height * 0.08)
                    width: Math.max(1, Math.min(preview.width - 16,
                                               (preview.height - 16) * 4 / 3))
                    height: width * 3 / 4
                    color: "transparent"
                    border.width: 2
                    border.color: "#ffcc48"
                    opacity: 0.9
                }
            }

            Image {
                objectName: root.objectName + "Watermark"
                anchors.centerIn: parent
                width: Math.max(1, Math.min(parent.width, parent.height) / 4)
                height: width
                source: Qt.resolvedUrl("../assets/icons/NewConvert9918-256.png")
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                opacity: 0.12
                visible: root.imageSource.toString().length === 0 && !root.busy
                Accessible.name: qsTr("New Convert 9918 logo")
            }

            Label {
                anchors.centerIn: parent
                visible: root.imageSource.toString().length === 0 && !root.busy
                width: Math.min(parent.width - 32, 360)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: root.emptyText
                color: "#c2c8cf"
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: root.busy
                visible: running
                Accessible.name: qsTr("Conversion in progress")
            }

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: event => root.zoomBy(event.angleDelta.y > 0 ? 1.15 : 0.87)
            }

            DropArea {
                anchors.fill: parent
                enabled: root.acceptDrops
                onDropped: drop => {
                    if (drop.urls.length > 0) {
                        root.fileDropped(drop.urls[0])
                        drop.acceptProposedAction()
                    }
                }
            }
        }

        RowLayout {
            objectName: root.objectName + "Controls"
            Layout.fillWidth: true

            Label {
                objectName: root.objectName + "Details"
                Layout.fillWidth: true
                text: root.details
                wrapMode: Text.WordWrap
                color: palette.placeholderText
            }
            ToolButton {
                objectName: root.objectName + "ZoomOutButton"
                text: qsTr("−")
                enabled: root.imageSource.toString().length > 0
                Accessible.name: qsTr("Zoom out %1").arg(root.title)
                activeFocusOnTab: true
                onClicked: root.zoomBy(0.8)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Zoom out")
            }
            ToolButton {
                objectName: root.objectName + "FitButton"
                text: qsTr("Fit")
                checkable: true
                checked: root.fitToView
                enabled: root.imageSource.toString().length > 0
                Accessible.name: qsTr("Fit %1 to view").arg(root.title)
                activeFocusOnTab: true
                onClicked: root.fitToView = true
            }
            ToolButton {
                objectName: root.objectName + "ActualSizeButton"
                text: qsTr("1:1")
                enabled: root.imageSource.toString().length > 0
                Accessible.name: qsTr("Show %1 at actual size").arg(root.title)
                activeFocusOnTab: true
                onClicked: {
                    root.fitToView = false
                    root.manualZoom = 1.0
                }
            }
            ToolButton {
                objectName: root.objectName + "ZoomInButton"
                text: qsTr("+")
                enabled: root.imageSource.toString().length > 0
                Accessible.name: qsTr("Zoom in %1").arg(root.title)
                activeFocusOnTab: true
                onClicked: root.zoomBy(1.25)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Zoom in")
            }
        }
    }
}

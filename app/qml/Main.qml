import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: qsTr("New Convert 9918")

    property url sourceUrl

    FileDialog {
        id: openDialog
        title: qsTr("Open source image")
        nameFilters: [
            qsTr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp)"),
            qsTr("All files (*)")
        ]
        onAccepted: window.sourceUrl = selectedFile
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12

            Label {
                text: qsTr("New Convert 9918")
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Open Image")
                onClicked: openDialog.open()
            }

            Button {
                text: qsTr("Export")
                enabled: window.sourceUrl.toString().length > 0
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Frame {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 320

                ColumnLayout {
                    anchors.fill: parent

                    Label {
                        text: qsTr("Source")
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: palette.alternateBase
                        border.color: palette.mid

                        Image {
                            anchors.fill: parent
                            anchors.margins: 12
                            source: window.sourceUrl
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: window.sourceUrl.toString().length === 0
                            text: qsTr("Drop an image here or choose Open Image")
                            color: palette.placeholderText
                        }

                        DropArea {
                            anchors.fill: parent
                            onDropped: drop => {
                                if (drop.urls.length > 0)
                                    window.sourceUrl = drop.urls[0]
                            }
                        }
                    }
                }
            }

            Frame {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 320

                ColumnLayout {
                    anchors.fill: parent

                    Label {
                        text: qsTr("Converted preview")
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#111111"
                        border.color: palette.mid

                        Label {
                            anchors.centerIn: parent
                            text: qsTr("Conversion engine coming next")
                            color: "#b8b8b8"
                        }
                    }
                }
            }
        }

        Frame {
            Layout.preferredWidth: 300
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                Label {
                    text: qsTr("Conversion")
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                Label { text: qsTr("Target mode") }
                ComboBox {
                    Layout.fillWidth: true
                    model: [
                        qsTr("Bitmap 9918A"),
                        qsTr("Greyscale Bitmap 9918A"),
                        qsTr("B&W Bitmap 9918A"),
                        qsTr("Multicolor 9918"),
                        qsTr("Dual Multicolor 9918"),
                        qsTr("Half Multicolor 9918A"),
                        qsTr("Paletted Bitmap F18A"),
                        qsTr("Scanline Palette Bitmap F18A")
                    ]
                }

                Label { text: qsTr("Dithering") }
                ComboBox {
                    Layout.fillWidth: true
                    model: [
                        qsTr("Floyd–Steinberg"),
                        qsTr("Atkinson"),
                        qsTr("Pattern"),
                        qsTr("Ordered"),
                        qsTr("None")
                    ]
                }

                CheckBox { text: qsTr("Perceptual color matching") }
                CheckBox { text: qsTr("Stretch histogram") }

                Label { text: qsTr("Maximum color shift") }
                Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: 1
                }

                Item { Layout.fillHeight: true }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Settings will update the preview automatically; no Reload step will be required.")
                    color: palette.placeholderText
                }
            }
        }
    }
}


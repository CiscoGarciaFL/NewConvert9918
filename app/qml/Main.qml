import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 1360
    height: 860
    minimumWidth: 720
    minimumHeight: 560
    visible: true
    title: imageInput.sourceName.length > 0
           ? qsTr("%1 — New Convert 9918").arg(imageInput.sourceName)
           : qsTr("New Convert 9918")
    // Normally tracks the window; kept as a separate property so embedded
    // hosts and automated layout checks can supply their available width.
    property real layoutWidth: width
    readonly property bool compactLayout: layoutWidth < 1100

    FileDialog {
        id: openDialog
        title: qsTr("Open source image")
        nameFilters: [
            qsTr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp *.pcx)"),
            qsTr("Retro images (*.tiap *.tiac *.tiam *_P *_C *_M *.sc2 *.pc *.pp *.hgr *.hgrh)"),
            qsTr("All files (*)")
        ]
        onAccepted: imageInput.openUrl(selectedFile)
    }

    FolderDialog {
        id: exportDialog
        title: qsTr("Choose export folder")
        onAccepted: imageInput.exportToDirectory(selectedFolder)
    }

    Dialog {
        id: overwriteDialog
        title: qsTr("Replace existing export files?")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        width: Math.min(window.width - 48, 620)
        onAccepted: imageInput.confirmOverwrite()
        onRejected: imageInput.cancelOverwrite()

        Label {
            width: parent.width
            text: imageInput.overwriteMessage
            wrapMode: Text.WrapAnywhere
        }
    }

    Connections {
        target: imageInput
        function onExportChanged() {
            if (imageInput.overwriteMessage.length > 0 && !overwriteDialog.opened)
                overwriteDialog.open()
        }
    }

    Dialog {
        id: aboutDialog
        title: qsTr("About New Convert 9918")
        modal: true
        standardButtons: Dialog.Close
        width: Math.min(window.width - 48, 620)

        ColumnLayout {
            width: parent.width
            spacing: 12
            Label {
                text: qsTr("New Convert 9918")
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("A cross-platform image converter for TMS9918A and F18A graphics.")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Convert9918 was designed and created by Mike Brent (Tursi) of HarmlessLion.com. New cross-platform architecture, interface, user experience, and features by Cisco Garcia / CiscoGarciaFL.")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                textFormat: Text.RichText
                text: qsTr("<a href='https://github.com/tursilion/convert9918'>Original Convert9918 project</a><br><a href='https://github.com/CiscoGarciaFL/NewConvert9918'>New Convert 9918 project</a><br><a href='http://harmlesslion.com'>HarmlessLion.com</a>")
                onLinkActivated: link => Qt.openUrlExternally(link)
                Accessible.name: qsTr("Project and attribution links")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Distributed under the original Convert9918 license terms with the original author's permission. See LICENSE and NOTICE.md for the complete terms and attribution.")
                color: palette.placeholderText
            }
        }
    }

    Shortcut { sequence: "Ctrl+O"; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+V"; onActivated: imageInput.pasteClipboard() }
    Shortcut { sequence: "Ctrl+E"; enabled: imageInput.hasConversion; onActivated: exportDialog.open() }
    Shortcut { sequence: "Ctrl+Z"; enabled: imageInput.canUndo; onActivated: imageInput.undoSettings() }
    Shortcut { sequence: "Ctrl+0"; onActivated: imageInput.resetSettings() }
    Shortcut { sequence: "F1"; onActivated: aboutDialog.open() }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8

            Label {
                text: qsTr("New Convert 9918")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Button {
                text: qsTr("Open")
                icon.name: "document-open"
                onClicked: openDialog.open()
                activeFocusOnTab: true
                Accessible.name: qsTr("Open source image")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Open image (Ctrl+O)")
            }
            Button {
                text: qsTr("Paste")
                visible: !window.compactLayout
                onClicked: imageInput.pasteClipboard()
                activeFocusOnTab: true
                Accessible.name: qsTr("Paste image from clipboard")
            }
            Button {
                text: qsTr("Export")
                enabled: imageInput.hasConversion
                onClicked: exportDialog.open()
                activeFocusOnTab: true
                Accessible.name: qsTr("Export converted image")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Export (Ctrl+E)")
            }
            Button {
                text: qsTr("Settings")
                visible: window.compactLayout
                onClicked: settingsDrawer.open()
                activeFocusOnTab: true
                Accessible.name: qsTr("Open conversion settings")
            }
            ToolButton {
                text: qsTr("About")
                onClicked: aboutDialog.open()
                activeFocusOnTab: true
                Accessible.name: qsTr("About New Convert 9918")
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        SplitView {
            id: previews
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: width < 820 ? Qt.Vertical : Qt.Horizontal

            PreviewPane {
                objectName: "sourcePreview"
                title: qsTr("Source")
                imageSource: imageInput.sourcePreview
                details: imageInput.hasImage
                         ? imageInput.sourceName + "\n" + imageInput.sourceDetails : ""
                emptyText: qsTr("Drop an image here or choose Open")
                acceptDrops: true
                cropOverlay: imageInput.hasImage && imageInput.fillMode !== 0
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                SplitView.minimumWidth: 260
                SplitView.minimumHeight: 180
                onFileDropped: fileUrl => imageInput.openUrl(fileUrl)
            }

            PreviewPane {
                objectName: "convertedPreview"
                title: qsTr("Converted preview")
                imageSource: imageInput.convertedPreview
                details: imageInput.conversionDetails
                emptyText: imageInput.hasImage
                           ? qsTr("The converted preview will appear here")
                           : qsTr("Open a source image to begin")
                busy: imageInput.busy
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                SplitView.minimumWidth: 260
                SplitView.minimumHeight: 180
            }
        }

        Frame {
            visible: !window.compactLayout
            Layout.preferredWidth: 350
            Layout.maximumWidth: 420
            Layout.fillHeight: true

            SettingsPanel {
                anchors.fill: parent
                onExportRequested: exportDialog.open()
            }
        }
    }

    Drawer {
        id: settingsDrawer
        edge: Qt.RightEdge
        width: Math.min(window.width * 0.9, 390)
        height: window.height
        modal: true

        SettingsPanel {
            anchors.fill: parent
            anchors.margins: 12
            onExportRequested: {
                settingsDrawer.close()
                exportDialog.open()
            }
        }
    }

    footer: ToolBar {
        height: Math.max(36, statusLabel.implicitHeight + 12)
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            Label {
                id: statusLabel
                Layout.fillWidth: true
                text: imageInput.errorMessage.length > 0
                      ? imageInput.errorMessage : imageInput.statusMessage
                color: imageInput.errorMessage.length > 0 ? "#ff8f8f" : palette.text
                wrapMode: Text.WordWrap
                Accessible.name: text
            }
            BusyIndicator {
                running: imageInput.busy
                visible: running
                implicitWidth: 24
                implicitHeight: 24
            }
        }
    }
}

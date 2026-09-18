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
    property int previewLayout: 1
    property bool conversionPanelVisible: true
    property int conversionPanelMode: 0

    function synchronizeConversionPanel() {
        if (conversionPanelMode === 1 && conversionPanelVisible)
            overlayConversionPanel.open()
        else
            overlayConversionPanel.close()
    }

    onConversionPanelVisibleChanged: Qt.callLater(synchronizeConversionPanel)
    onConversionPanelModeChanged: Qt.callLater(synchronizeConversionPanel)
    Component.onCompleted: synchronizeConversionPanel()

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

    Action {
        id: openAction
        text: qsTr("&Open…")
        shortcut: "Ctrl+O"
        onTriggered: openDialog.open()
    }
    Action {
        id: pasteAction
        text: qsTr("&Paste")
        shortcut: "Ctrl+V"
        onTriggered: imageInput.pasteClipboard()
    }
    Action {
        id: exportAction
        text: qsTr("&Export…")
        shortcut: "Ctrl+E"
        enabled: imageInput.hasConversion
        onTriggered: exportDialog.open()
    }
    Action {
        id: exitAction
        objectName: "exitAction"
        text: qsTr("E&xit")
        shortcut: StandardKey.Quit
        onTriggered: window.close()
    }
    Action {
        id: aboutAction
        text: qsTr("&About New Convert 9918")
        shortcut: "F1"
        onTriggered: aboutDialog.open()
    }

    Shortcut { sequence: "Ctrl+Z"; enabled: imageInput.canUndo; onActivated: imageInput.undoSettings() }
    Shortcut { sequence: "Ctrl+0"; onActivated: imageInput.resetSettings() }

    menuBar: MenuBar {
        objectName: "mainMenuBar"

        Menu {
            title: qsTr("&File")
            MenuItem { action: openAction }
            MenuItem { action: pasteAction }
            MenuSeparator {}
            MenuItem { action: exportAction }
            MenuSeparator { objectName: "exitMenuSeparator" }
            MenuItem {
                objectName: "exitMenuItem"
                action: exitAction
            }
        }

        Menu {
            title: qsTr("&View")

            MenuItem {
                text: qsTr("&Tabbed")
                checkable: true
                checked: window.previewLayout === 0
                onTriggered: window.previewLayout = 0
            }
            MenuItem {
                text: qsTr("&Horizontal")
                checkable: true
                checked: window.previewLayout === 1
                onTriggered: window.previewLayout = 1
            }
            MenuItem {
                text: qsTr("&Vertical")
                checkable: true
                checked: window.previewLayout === 2
                onTriggered: window.previewLayout = 2
            }

            MenuSeparator {}

            MenuItem {
                text: qsTr("Show &Conversion Panel")
                checkable: true
                checked: window.conversionPanelVisible
                onTriggered: window.conversionPanelVisible = !window.conversionPanelVisible
            }

            Menu {
                title: qsTr("Conversion Panel &Placement")
                MenuItem {
                    text: qsTr("&Adjacent")
                    checkable: true
                    checked: window.conversionPanelMode === 0
                    onTriggered: {
                        window.conversionPanelMode = 0
                        window.conversionPanelVisible = true
                    }
                }
                MenuItem {
                    text: qsTr("&Overlay")
                    checkable: true
                    checked: window.conversionPanelMode === 1
                    onTriggered: {
                        window.conversionPanelMode = 1
                        window.conversionPanelVisible = true
                    }
                }
            }
        }

        Menu {
            title: qsTr("&Help")
            MenuItem { action: aboutAction }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        PreviewWorkspace {
            id: previews
            objectName: "previewWorkspace"
            layoutMode: window.previewLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Frame {
            objectName: "adjacentConversionPanel"
            visible: window.conversionPanelVisible && window.conversionPanelMode === 0
            Layout.preferredWidth: 350
            Layout.maximumWidth: 420
            Layout.fillHeight: true

            SettingsPanel {
                anchors.fill: parent
                onExportRequested: exportDialog.open()
            }
        }
    }

    Frame {
        id: overlayExpandRail
        objectName: "overlayExpandRail"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 38
        padding: 0
        z: 20
        visible: window.conversionPanelMode === 1 && !window.conversionPanelVisible

        ToolButton {
            objectName: "overlayExpandButton"
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: 6
            text: qsTr("‹")
            font.pixelSize: 24
            activeFocusOnTab: true
            Accessible.name: qsTr("Show Conversion panel")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: window.conversionPanelVisible = true
        }
    }

    Drawer {
        id: overlayConversionPanel
        objectName: "overlayConversionPanel"
        edge: Qt.RightEdge
        width: Math.min(window.width * 0.9, 390)
        height: window.height
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape
        property bool pointerWasInside: false
        function handlePointerPresence(inside) {
            if (inside) {
                pointerWasInside = true
            } else if (pointerWasInside && opened
                       && window.conversionPanelMode === 1) {
                window.conversionPanelVisible = false
            }
        }
        onOpened: pointerWasInside = false
        onClosed: {
            if (window.conversionPanelMode === 1 && window.conversionPanelVisible)
                window.conversionPanelVisible = false
        }

        HoverHandler {
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: overlayConversionPanel.handlePointerPresence(hovered)
        }

        SettingsPanel {
            anchors.fill: parent
            anchors.margins: 12
            closable: true
            onExportRequested: exportDialog.open()
            onCloseRequested: window.conversionPanelVisible = false
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

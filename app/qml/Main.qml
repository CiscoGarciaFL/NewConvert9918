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
    title: qsTr("New Convert 9918")
    // Normally tracks the window; kept as a separate property so embedded
    // hosts and automated layout checks can supply their available width.
    property real layoutWidth: width
    readonly property bool compactLayout: layoutWidth < 1100
    property int previewLayout: appPreferences.previewLayout
    // 0 = Screen Image, 1 = Character Editor, 2 = Sprite Editor.
    property int workspaceMode: 0
    property bool conversionPanelVisible: appPreferences.sidePanelVisible
    property int conversionPanelMode: appPreferences.sidePanelMode

    function synchronizeConversionPanel() {
        if (conversionPanelMode === 1 && conversionPanelVisible)
            overlayConversionPanel.open()
        else
            overlayConversionPanel.close()
    }

    onPreviewLayoutChanged: {
        if (appPreferences.previewLayout !== previewLayout)
            appPreferences.previewLayout = previewLayout
    }
    onConversionPanelVisibleChanged: {
        if (appPreferences.sidePanelVisible !== conversionPanelVisible)
            appPreferences.sidePanelVisible = conversionPanelVisible
        Qt.callLater(synchronizeConversionPanel)
    }
    onConversionPanelModeChanged: {
        if (appPreferences.sidePanelMode !== conversionPanelMode)
            appPreferences.sidePanelMode = conversionPanelMode
        Qt.callLater(synchronizeConversionPanel)
    }
    onWorkspaceModeChanged: {
        if (editorProject.workspaceMode !== workspaceMode)
            editorProject.workspaceMode = workspaceMode
        if (appPreferences.rememberWorkspaceMode
                && appPreferences.lastWorkspaceMode !== workspaceMode)
            appPreferences.lastWorkspaceMode = workspaceMode
    }
    onClosing: close => appPreferences.saveWindowGeometry(x, y, width, height)
    Component.onCompleted: {
        if (appPreferences.restoreWindowGeometry
                && appPreferences.hasWindowGeometry) {
            window.x = appPreferences.windowX
            window.y = appPreferences.windowY
            window.width = appPreferences.windowWidth
            window.height = appPreferences.windowHeight
        }
        if (appPreferences.rememberWorkspaceMode)
            window.workspaceMode = appPreferences.lastWorkspaceMode
        synchronizeConversionPanel()
    }

    Connections {
        target: editorProject
        function onProjectChanged() {
            if (window.workspaceMode !== editorProject.workspaceMode)
                window.workspaceMode = editorProject.workspaceMode
        }
    }

    Connections {
        target: appPreferences
        function onPreferencesChanged() {
            if (window.previewLayout !== appPreferences.previewLayout)
                window.previewLayout = appPreferences.previewLayout
            if (window.conversionPanelVisible !== appPreferences.sidePanelVisible)
                window.conversionPanelVisible = appPreferences.sidePanelVisible
            if (window.conversionPanelMode !== appPreferences.sidePanelMode)
                window.conversionPanelMode = appPreferences.sidePanelMode
        }
    }

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

    PreferencesDialog {
        id: preferencesDialog
        applicationWindow: window
    }

    FileDialog {
        id: loadRecipeDialog
        title: qsTr("Load conversion recipe")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("New Convert 9918 recipes (*.nc9918.json *.json)"),
                      qsTr("All files (*)")]
        onAccepted: editorProject.loadRecipe(selectedFile)
    }

    FileDialog {
        id: saveRecipeDialog
        title: qsTr("Save conversion recipe")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "nc9918.json"
        nameFilters: [qsTr("New Convert 9918 recipes (*.nc9918.json)"),
                      qsTr("JSON files (*.json)")]
        onAccepted: editorProject.saveRecipe(selectedFile)
    }
    Action {
        id: reloadAction
        objectName: "reloadAction"
        text: qsTr("&Reload")
        shortcut: "Ctrl+R"
        enabled: imageInput.canReload
        onTriggered: imageInput.reloadSource()
    }
    Action {
        id: exportAction
        objectName: "exportAction"
        text: qsTr("&Export…")
        shortcut: "Ctrl+E"
        enabled: window.workspaceMode === 0 && imageInput.hasConversion
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

    Shortcut {
        sequence: "Ctrl+Z"
        enabled: imageInput.canUndoDrawing || imageInput.canUndo
        onActivated: {
            if (imageInput.canUndoDrawing)
                imageInput.undoDrawing()
            else
                imageInput.undoSettings()
        }
    }
    Action {
        id: preferencesAction
        objectName: "preferencesAction"
        text: qsTr("&Preferences…")
        shortcut: "Ctrl+,"
        onTriggered: preferencesDialog.open()
    }
    Action {
        id: loadRecipeAction
        objectName: "loadRecipeAction"
        text: qsTr("&Load Recipe…")
        shortcut: "Ctrl+Shift+O"
        onTriggered: loadRecipeDialog.open()
    }
    Action {
        id: saveRecipeAction
        objectName: "saveRecipeAction"
        text: qsTr("&Save Recipe…")
        shortcut: "Ctrl+Shift+S"
        onTriggered: saveRecipeDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+Y"
        enabled: imageInput.canRedoDrawing
        onActivated: imageInput.redoDrawing()
    }
    Shortcut { sequence: "Ctrl+0"; onActivated: imageInput.resetSettings() }

    menuBar: MenuBar {
        objectName: "mainMenuBar"

        Menu {
            title: qsTr("&File")
            MenuItem { action: openAction }
            MenuItem {
                objectName: "reloadMenuItem"
                action: reloadAction
            }
            MenuItem { action: pasteAction }
            MenuSeparator {}
            MenuItem { action: loadRecipeAction }
            MenuItem { action: saveRecipeAction }
            MenuSeparator {}
            MenuItem { action: exportAction }
            MenuSeparator { objectName: "exitMenuSeparator" }
            MenuItem {
                objectName: "exitMenuItem"
                action: exitAction
            }
        }

        Menu {
            objectName: "modeMenu"
            title: qsTr("&Mode")

            MenuItem {
                objectName: "screenImageModeMenuItem"
                text: qsTr("&Screen Image")
                checkable: true
                checked: window.workspaceMode === 0
                onTriggered: window.workspaceMode = 0
            }
            MenuItem {
                objectName: "characterEditorModeMenuItem"
                text: qsTr("&Character Editor")
                checkable: true
                checked: window.workspaceMode === 1
                onTriggered: window.workspaceMode = 1
            }
            MenuItem {
                objectName: "spriteEditorModeMenuItem"
                text: qsTr("S&prite Editor")
                checkable: true
                checked: window.workspaceMode === 2
                onTriggered: window.workspaceMode = 2
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
                objectName: "showSidePanelMenuItem"
                text: qsTr("Show &Side Panel")
                checkable: true
                checked: window.conversionPanelVisible
                onTriggered: window.conversionPanelVisible = !window.conversionPanelVisible
            }

            Menu {
                objectName: "sidePanelPlacementMenu"
                title: qsTr("Side Panel &Placement")
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
            title: qsTr("&Settings")
            MenuItem { action: preferencesAction }
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
            workspaceMode: window.workspaceMode
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        ThemedFrame {
            objectName: "adjacentConversionPanel"
            visible: window.conversionPanelVisible && window.conversionPanelMode === 0
            Layout.preferredWidth: 350
            Layout.maximumWidth: 420
            Layout.fillHeight: true

            SidePanelHost {
                anchors.fill: parent
                workspaceMode: window.workspaceMode
                onExportRequested: exportDialog.open()
            }
        }
    }

    Item {
        id: overlayExpandRail
        objectName: "overlayExpandRail"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 38
        z: 20
        visible: window.conversionPanelMode === 1 && !window.conversionPanelVisible

        ToolButton {
            id: overlayExpandButton
            objectName: "overlayExpandButton"
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: 6
            text: qsTr("‹")
            font.pixelSize: 24
            background: Rectangle {
                objectName: "overlayExpandButtonBackground"
                radius: 3
                color: overlayExpandButton.down
                       ? overlayExpandButton.palette.mid
                       : overlayExpandButton.hovered
                         ? overlayExpandButton.palette.light
                         : overlayExpandButton.palette.button
                border.width: 1
                border.color: overlayExpandButton.palette.mid
            }
            activeFocusOnTab: true
            Accessible.name: qsTr("Show Side Panel")
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
        background: Rectangle {
            color: overlayConversionPanel.palette.window
            border.width: 1
            border.color: overlayConversionPanel.palette.windowText
        }
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

        SidePanelHost {
            anchors.fill: parent
            anchors.margins: 12
            workspaceMode: window.workspaceMode
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
                id: sourceNameLabel
                objectName: "statusSourceName"
                visible: text.length > 0
                Layout.maximumWidth: Math.max(120, window.width * 0.4)
                text: imageInput.sourceName
                elide: Text.ElideMiddle
                Accessible.name: qsTr("Source file: %1").arg(text)
                ToolTip.visible: sourceNameHover.hovered && truncated
                ToolTip.text: text

                HoverHandler {
                    id: sourceNameHover
                }
            }
            ToolSeparator {
                visible: sourceNameLabel.visible
            }
            Label {
                id: statusLabel
                Layout.fillWidth: true
                text: editorProject.errorMessage.length > 0
                      ? editorProject.errorMessage
                      : editorProject.statusMessage.length > 0
                        ? editorProject.statusMessage
                        : imageInput.errorMessage.length > 0
                          ? imageInput.errorMessage : imageInput.statusMessage
                color: editorProject.errorMessage.length > 0
                       || imageInput.errorMessage.length > 0
                       ? "#ff8f8f" : palette.text
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

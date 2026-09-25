import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string objectPrefix: "outputProfiles"
    spacing: 5

    CheckBox {
        objectName: root.objectPrefix + "Tms9918aCheckBox"
        Layout.fillWidth: true
        text: qsTr("TMS9918A baseline")
        checked: true
        enabled: false
        Accessible.description: qsTr("Every editor project keeps a TMS9918A-compatible baseline")
    }
    CheckBox {
        objectName: root.objectPrefix + "F18aCheckBox"
        Layout.fillWidth: true
        text: qsTr("F18A enhanced output")
        checked: editorProject.f18aEnabled
        onToggled: editorProject.f18aEnabled = checked
    }
    Label {
        Layout.fillWidth: true
        text: editorProject.f18aEnabled
              ? qsTr("F18A inherits the baseline until an enhanced option is changed.")
              : qsTr("Only the TMS9918A-compatible result will be exported.")
        wrapMode: Text.WordWrap
        color: palette.placeholderText
    }
    Label {
        text: qsTr("Preview")
        font.weight: Font.DemiBold
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 1

        ButtonGroup { id: previewGroup; exclusive: true }
        ToolButton {
            objectName: root.objectPrefix + "Preview9918Button"
            Layout.fillWidth: true
            text: qsTr("9918A")
            checkable: true
            checked: editorProject.previewTarget === 0
            ButtonGroup.group: previewGroup
            onClicked: editorProject.previewTarget = 0
        }
        ToolButton {
            objectName: root.objectPrefix + "PreviewF18aButton"
            Layout.fillWidth: true
            text: qsTr("F18A")
            enabled: editorProject.f18aEnabled
            checkable: true
            checked: editorProject.previewTarget === 1
            ButtonGroup.group: previewGroup
            onClicked: editorProject.previewTarget = 1
        }
        ToolButton {
            objectName: root.objectPrefix + "PreviewCompareButton"
            Layout.fillWidth: true
            text: qsTr("Compare")
            enabled: editorProject.f18aEnabled
            checkable: true
            checked: editorProject.previewTarget === 2
            ButtonGroup.group: previewGroup
            onClicked: editorProject.previewTarget = 2
        }
    }
}

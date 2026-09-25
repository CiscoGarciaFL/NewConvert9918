import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        TabBar {
            id: setTabs
            objectName: "characterSetTabs"
            Layout.fillWidth: true
            currentIndex: editorProject.activeCharacterSet
            onCurrentIndexChanged: editorProject.activeCharacterSet = currentIndex

            Repeater {
                model: editorProject.characterSetNames
                TabButton {
                    required property string modelData
                    text: modelData
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("256 patterns · selected pattern %1")
                  .arg(editorProject.activeCharacterPattern)
            color: "#c2c8cf"
        }

        GridView {
            id: patternGrid
            objectName: "characterPatternGrid"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: 256
            clip: true
            cellWidth: Math.max(28, Math.floor(width / 16))
            cellHeight: cellWidth
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                required property int index
                width: patternGrid.cellWidth - 2
                height: patternGrid.cellHeight - 2
                color: index === editorProject.activeCharacterPattern
                       ? "#315f82" : "#1b2229"
                border.width: index === editorProject.activeCharacterPattern ? 2 : 1
                border.color: index === editorProject.activeCharacterPattern
                              ? "#8fd0ff" : "#53606d"
                radius: 2

                Text {
                    anchors.centerIn: parent
                    text: parent.index.toString(16).toUpperCase().padStart(2, "0")
                    color: "#d8dde3"
                    font.pixelSize: Math.max(13, Math.min(17, parent.width * 0.28 + 5))
                }
                TapHandler {
                    onTapped: editorProject.activeCharacterPattern = parent.index
                }
            }
        }
    }
}

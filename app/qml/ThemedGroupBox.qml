import QtQuick
import QtQuick.Controls

GroupBox {
    id: control

    readonly property color outlineColor: palette.windowText

    background: Rectangle {
        y: control.topPadding - control.bottomPadding
        width: parent.width
        height: parent.height - control.topPadding + control.bottomPadding
        color: "transparent"
        radius: 2
        border.width: 1
        border.color: control.outlineColor
    }
}

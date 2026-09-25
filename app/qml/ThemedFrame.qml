import QtQuick
import QtQuick.Controls

Frame {
    id: control

    readonly property color outlineColor: palette.windowText

    background: Rectangle {
        color: "transparent"
        radius: 2
        border.width: 1
        border.color: control.outlineColor
    }
}

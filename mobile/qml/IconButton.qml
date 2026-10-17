import QtQuick
import QtQuick.Controls

// A round, touch-sized button with one of the painted icons.
AbstractButton {
    id: button
    property string glyph
    property bool filledIcon: false
    property color iconColor: Theme.text
    implicitWidth: Theme.touch
    implicitHeight: Theme.touch
    opacity: enabled ? 1 : 0.35

    background: Rectangle {
        radius: width / 2
        color: button.pressed ? Theme.pressed : "transparent"
    }
    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            width: 24
            height: 24
            name: button.glyph
            color: button.iconColor
            filled: button.filledIcon
        }
    }
}

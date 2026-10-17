import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SpacePenguin

// One row of the menu or settings: an icon, a label, and optionally a switch.
AbstractButton {
    id: row
    property string glyph
    property string label
    property string detail
    property bool toggle: false
    property bool filledIcon: false
    implicitHeight: detail ? 68 : 56
    checkable: toggle
    background: Rectangle { color: row.pressed ? Theme.pressed : "transparent" }
    contentItem: RowLayout {
        spacing: 16
        Item { Layout.preferredWidth: 4 }
        Icon {
            // An empty glyph still keeps its column, so labels line up.
            opacity: row.glyph !== "" ? 1 : 0
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            name: row.glyph
            color: Theme.text
            filled: row.filledIcon
        }
        Column {
            Layout.fillWidth: true
            spacing: 2
            Text { text: row.label; color: Theme.text; font.pixelSize: 16; width: parent.width; elide: Text.ElideRight }
            Text {
                visible: row.detail !== ""
                text: row.detail
                color: Theme.muted
                font.pixelSize: 13
                width: parent.width
                wrapMode: Text.Wrap
            }
        }
        Switch {
            visible: row.toggle
            checked: row.checked
            // A tap on the switch counts as a tap on the row, which owns the state.
            onClicked: {
                checked = Qt.binding(() => row.checked);
                row.click();
            }
        }
        Item { Layout.preferredWidth: 4 }
    }
}

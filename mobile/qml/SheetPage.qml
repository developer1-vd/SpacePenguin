import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SpacePenguin

// A full-screen page with a title and a close button; the menu, tabs, bookmarks,
// history and settings are all one of these.
Rectangle {
    id: sheet
    property string title
    default property alias content: body.data
    property alias headerActions: actions.data
    signal closeRequested()

    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.leftMargin: 20
            Layout.rightMargin: 6
            Text {
                Layout.fillWidth: true
                text: sheet.title
                color: Theme.text
                font.pixelSize: 22
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            Row { id: actions }
            IconButton { glyph: "close"; onClicked: sheet.closeRequested() }
        }

        Item {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}

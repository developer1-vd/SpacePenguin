import QtQuick
import QtQuick.Controls
import SpacePenguin

// Bookmarks or history: title and address per row, tap to open.
ListView {
    id: list
    property string emptyText
    property bool removable: false
    signal opened(string url)
    signal removed(string url)

    clip: true
    boundsBehavior: Flickable.StopAtBounds

    delegate: AbstractButton {
        id: row
        required property var modelData
        width: ListView.view.width
        height: 66
        onClicked: list.opened(modelData.url)
        background: Rectangle { color: row.pressed ? Theme.pressed : "transparent" }
        contentItem: Item {
            Column {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.right: parent.right
                anchors.rightMargin: list.removable ? 60 : 20
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text {
                    width: parent.width
                    text: row.modelData.title
                    color: Theme.text
                    font.pixelSize: 16
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: BrowserBackend.displayUrl(row.modelData.url)
                    color: Theme.muted
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                }
            }
            IconButton {
                visible: list.removable
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                glyph: "trash"
                iconColor: Theme.muted
                onClicked: list.removed(row.modelData.url)
            }
        }
    }

    Text {
        visible: list.count === 0
        anchors.centerIn: parent
        width: parent.width - 60
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: list.emptyText
        color: Theme.muted
        font.pixelSize: 15
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SpacePenguin

// The new-tab page: the mark, a search field, and the bookmarks as tiles.
Rectangle {
    id: page
    color: Theme.background
    signal openRequested(string text)

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight + 48
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: column
            width: Math.min(parent.width - 40, 560)
            x: (parent.width - width) / 2
            y: Math.max(32, page.height * 0.12)
            spacing: 22

            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/icons/spacepenguin-256.png"
                sourceSize: Qt.size(88, 88)
                Layout.preferredWidth: 88
                Layout.preferredHeight: 88
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                textFormat: Text.StyledText
                text: "Space<font color=\"" + Theme.accent + "\">Penguin</font>"
                color: Theme.text
                font.pixelSize: 30
                font.weight: Font.Bold
            }

            TextField {
                id: search
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                leftPadding: 48
                rightPadding: 18
                placeholderText: qsTr("Search %1 or type an address").arg(BrowserBackend.searchEngineName)
                placeholderTextColor: Theme.muted
                color: Theme.text
                font.pixelSize: 17
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                background: Rectangle {
                    radius: height / 2
                    color: Theme.surface
                    border.color: search.activeFocus ? Theme.accent : Theme.border
                    border.width: search.activeFocus ? 2 : 1
                }
                Icon {
                    x: 16; anchors.verticalCenter: parent.verticalCenter
                    width: 22; height: 22; name: "search"; color: Theme.muted
                }
                onAccepted: if (text.trim()) { page.openRequested(text); text = ""; }
            }

            Text {
                visible: BrowserBackend.bookmarks.length > 0
                text: qsTr("Bookmarks")
                color: Theme.muted
                font.pixelSize: 14
                font.weight: Font.DemiBold
                Layout.topMargin: 10
            }

            GridLayout {
                Layout.fillWidth: true
                columns: Math.max(2, Math.floor(column.width / 130))
                columnSpacing: 10
                rowSpacing: 10
                Repeater {
                    model: BrowserBackend.bookmarks.slice(0, 12)
                    delegate: AbstractButton {
                        id: tile
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        onClicked: page.openRequested(modelData.url)
                        background: Rectangle {
                            radius: 16
                            color: tile.pressed ? Theme.pressed : Theme.surface
                            border.color: Theme.border
                        }
                        contentItem: Text {
                            text: tile.modelData.title
                            color: Theme.text
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 14
                            rightPadding: 14
                        }
                    }
                }
            }
        }
    }
}

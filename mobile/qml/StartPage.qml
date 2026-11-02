import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SpacePenguin

// The new-tab page: the mark, a search field, the most visited sites, the
// bookmarks and the latest pages. Laid out for a phone held either way.
Rectangle {
    id: page
    color: Theme.background
    signal openRequested(string text)

    // Narrow screens (a phone upright) get a smaller mark and four shortcuts a row.
    readonly property bool compact: width < 520

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.implicitHeight + column.y + 32
        boundsBehavior: Flickable.StopAtBounds
        // Tapping outside the field puts the keyboard away.
        onMovementStarted: search.focus = false

        ColumnLayout {
            id: column
            width: Math.min(parent.width - 32, 600)
            x: (parent.width - width) / 2
            // Lower on a tall screen, near the top in landscape or with the keyboard up.
            y: page.height > 600 ? page.height * 0.08 : 16
            spacing: 18

            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/icons/spacepenguin-256.png"
                readonly property int side: page.compact ? 64 : 88
                sourceSize: Qt.size(side, side)
                Layout.preferredWidth: side
                Layout.preferredHeight: side
                visible: page.height > 420
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                textFormat: Text.StyledText
                text: "Space<font color=\"" + Theme.accent + "\">Penguin</font>"
                color: Theme.text
                font.pixelSize: page.compact ? 26 : 30
                font.weight: Font.Bold
                visible: page.height > 420
            }

            TextField {
                id: search
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                leftPadding: 48
                rightPadding: 18
                // Short enough not to be cut off on a phone.
                placeholderText: qsTr("Search or type an address")
                placeholderTextColor: Theme.muted
                color: Theme.text
                font.pixelSize: 17
                // Not "URL characters only": many phone keyboards then drop the
                // space bar, and this field searches too.
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhPreferLowercase | Qt.ImhNoPredictiveText
                EnterKey.type: Qt.EnterKeyGo
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
                onAccepted: page.submit()
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                visible: BrowserBackend.searchEngineName.length > 0
                text: qsTr("Searches with %1").arg(BrowserBackend.searchEngineName)
                color: Theme.muted
                font.pixelSize: 13
                Layout.topMargin: -8
            }

            // ---- Most visited --------------------------------------------------
            GridLayout {
                id: shortcuts
                Layout.fillWidth: true
                Layout.topMargin: 6
                visible: BrowserBackend.topSites.length > 0
                columns: page.compact ? 4 : 6
                columnSpacing: 4
                rowSpacing: 10
                Repeater {
                    model: BrowserBackend.topSites
                    delegate: AbstractButton {
                        id: shortcut
                        required property var modelData
                        required property int index
                        // Fixed columns, so one or two shortcuts start at the left.
                        Layout.preferredWidth: (column.width - shortcuts.columnSpacing * (shortcuts.columns - 1))
                                               / shortcuts.columns
                        Layout.preferredHeight: 88
                        onClicked: page.openRequested(modelData.url)
                        background: Rectangle {
                            radius: 16
                            color: shortcut.pressed ? Theme.pressed : "transparent"
                        }
                        contentItem: Column {
                            spacing: 8
                            topPadding: 6
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 52; height: 52; radius: 26
                                color: Theme.surface
                                border.color: Theme.border
                                Text {
                                    anchors.centerIn: parent
                                    text: shortcut.modelData.letter
                                    color: Theme.groupColor(1 + shortcut.index % 8)
                                    font.pixelSize: 22
                                    font.weight: Font.Bold
                                }
                            }
                            Text {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: shortcut.modelData.title
                                color: Theme.text
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            // ---- Bookmarks -----------------------------------------------------
            Text {
                visible: BrowserBackend.bookmarks.length > 0
                text: qsTr("Bookmarks")
                color: Theme.muted
                font.pixelSize: 14
                font.weight: Font.DemiBold
                Layout.topMargin: 4
            }

            GridLayout {
                Layout.fillWidth: true
                visible: BrowserBackend.bookmarks.length > 0
                columns: Math.max(2, Math.floor(column.width / 150))
                columnSpacing: 10
                rowSpacing: 10
                Repeater {
                    model: BrowserBackend.bookmarks.slice(0, 8)
                    delegate: AbstractButton {
                        id: tile
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1   // equal columns, whatever the titles
                        Layout.preferredHeight: 60
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

            // ---- Recent --------------------------------------------------------
            Text {
                visible: recent.count > 0
                text: qsTr("Recent")
                color: Theme.muted
                font.pixelSize: 14
                font.weight: Font.DemiBold
                Layout.topMargin: 4
            }

            Rectangle {
                Layout.fillWidth: true
                visible: recent.count > 0
                implicitHeight: recentColumn.implicitHeight
                radius: 16
                color: Theme.surface
                border.color: Theme.border
                clip: true

                Column {
                    id: recentColumn
                    width: parent.width
                    Repeater {
                        id: recent
                        model: BrowserBackend.history.slice(0, 5)
                        delegate: AbstractButton {
                            id: row
                            required property var modelData
                            width: recentColumn.width
                            height: 60
                            onClicked: page.openRequested(modelData.url)
                            background: Rectangle { color: row.pressed ? Theme.pressed : "transparent"; radius: 16 }
                            contentItem: Column {
                                leftPadding: 16
                                rightPadding: 16
                                topPadding: 10
                                spacing: 2
                                Text {
                                    width: row.width - 32
                                    text: row.modelData.title
                                    color: Theme.text
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: row.width - 32
                                    text: BrowserBackend.displayUrl(row.modelData.url)
                                    color: Theme.muted
                                    font.pixelSize: 12
                                    elide: Text.ElideMiddle
                                }
                            }
                        }
                    }
                }
            }

            // A first start has nothing to show yet; say what will appear.
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 12
                visible: BrowserBackend.history.length === 0 && BrowserBackend.bookmarks.length === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("The sites you visit most and the pages you bookmark will show up here.")
                color: Theme.muted
                font.pixelSize: 14
            }
        }
    }

    function submit() {
        const text = search.text.trim();
        if (!text)
            return;
        search.focus = false;
        Qt.inputMethod.hide();
        page.openRequested(text);
        search.text = "";
    }
}

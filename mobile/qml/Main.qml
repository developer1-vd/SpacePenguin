import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SpacePenguin

ApplicationWindow {
    id: window
    width: 420
    height: 860
    visible: true
    title: "SpacePenguin"
    color: Theme.background

    // "", "menu", "tabs", "bookmarks", "history", "settings", "about"
    property string sheet: ""
    property bool editing: false
    property int current: 0
    readonly property var tab: tabRepeater.count > current ? tabRepeater.itemAt(current) : null

    ListModel { id: tabModel }

    function newTab(address) {
        tabModel.append({ first: address || "" });
        current = tabModel.count - 1;
        sheet = "";
        editing = false;
    }
    function closeTab(index) {
        tabModel.remove(index);
        if (tabModel.count === 0)
            newTab("");
        current = Math.min(current, tabModel.count - 1);
    }
    function open(text) {
        editing = false;
        sheet = "";
        if (tab)
            tab.load(BrowserBackend.resolve(text));
    }

    Component.onCompleted: newTab("")

    // Android's Back button closes what is open, then goes back, then leaves.
    onClosing: close => {
        if (sheet !== "") { sheet = ""; close.accepted = false; }
        else if (editing) { editing = false; close.accepted = false; }
        else if (tab && !tab.onStartPage) { tab.goBack(); close.accepted = false; }
    }

    // ---- Pages -----------------------------------------------------------
    Item {
        id: pages
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: toolbar.top

        Repeater {
            id: tabRepeater
            model: tabModel
            delegate: BrowserTab {
                required property int index
                required property string first
                anchors.fill: parent
                visible: index === window.current
                pageVisible: window.sheet === "" && !window.editing
                onOpenRequested: text => window.open(text)
                Component.onCompleted: if (first) load(BrowserBackend.resolve(first))
            }
        }

        // While the address is being edited the page is covered by suggestions
        // from bookmarks and history, so the native page view is hidden.
        Rectangle {
            anchors.fill: parent
            visible: window.editing && window.tab && !window.tab.onStartPage
            color: Theme.background
            LinkList {
                anchors.fill: parent
                anchors.topMargin: 8
                model: {
                    const typed = addressField.text.toLowerCase();
                    const all = BrowserBackend.bookmarks.concat(BrowserBackend.history);
                    const seen = {};
                    return all.filter(e => {
                        if (seen[e.url]) return false;
                        seen[e.url] = true;
                        return !typed || e.title.toLowerCase().includes(typed) || e.url.toLowerCase().includes(typed);
                    }).slice(0, 30);
                }
                emptyText: qsTr("Press Go to search %1.").arg(BrowserBackend.searchEngineName)
                onOpened: url => window.open(url)
            }
        }
    }

    // ---- Full-screen sheets ------------------------------------------------
    SheetPage {
        anchors.fill: pages
        visible: window.sheet === "menu"
        title: window.tab ? window.tab.title : ""
        onCloseRequested: window.sheet = ""
        headerActions: [
            IconButton {
                glyph: "star"
                enabled: window.tab && !window.tab.onStartPage
                filledIcon: window.tab && BrowserBackend.bookmarks && BrowserBackend.isBookmarked(window.tab.url)
                iconColor: filledIcon ? Theme.accent : Theme.text
                onClicked: BrowserBackend.toggleBookmark(window.tab.title, window.tab.url)
            }
        ]
        Flickable {
            anchors.fill: parent
            contentHeight: menuColumn.implicitHeight
            clip: true
            Column {
                id: menuColumn
                width: parent.width
                MenuRow { width: parent.width; glyph: "plus"; label: qsTr("New tab"); onClicked: window.newTab("") }
                MenuRow { width: parent.width; glyph: "bookmarks"; label: qsTr("Bookmarks"); onClicked: window.sheet = "bookmarks" }
                MenuRow { width: parent.width; glyph: "history"; label: qsTr("History"); onClicked: window.sheet = "history" }
                MenuRow {
                    width: parent.width; glyph: "share"; label: qsTr("Open in another app")
                    enabled: window.tab && !window.tab.onStartPage
                    onClicked: { Qt.openUrlExternally(window.tab.url); window.sheet = ""; }
                }
                MenuRow { width: parent.width; glyph: "settings"; label: qsTr("Settings"); onClicked: window.sheet = "settings" }
                MenuRow { width: parent.width; glyph: "info"; label: qsTr("About SpacePenguin"); onClicked: window.sheet = "about" }
            }
        }
    }

    SheetPage {
        anchors.fill: pages
        visible: window.sheet === "tabs"
        title: tabModel.count === 1 ? qsTr("1 tab") : qsTr("%1 tabs").arg(tabModel.count)
        onCloseRequested: window.sheet = ""
        headerActions: [ IconButton { glyph: "plus"; onClicked: window.newTab("") } ]
        GridView {
            anchors.fill: parent
            anchors.margins: 12
            clip: true
            cellWidth: width / Math.max(2, Math.floor(width / 180))
            cellHeight: 120
            model: tabModel
            delegate: Item {
                id: cell
                required property int index
                width: GridView.view.cellWidth
                height: GridView.view.cellHeight
                AbstractButton {
                    id: card
                    anchors.fill: parent
                    anchors.margins: 6
                    onClicked: { window.current = cell.index; window.sheet = ""; }
                    background: Rectangle {
                        radius: Theme.radius
                        color: card.pressed ? Theme.pressed : Theme.surface
                        border.color: cell.index === window.current ? Theme.accent : Theme.border
                        border.width: cell.index === window.current ? 2 : 1
                    }
                    contentItem: Item {
                        readonly property var page: tabRepeater.itemAt(cell.index)
                        Text {
                            anchors.fill: parent
                            anchors.margins: 14
                            anchors.rightMargin: 40
                            text: parent.page ? parent.page.title : ""
                            color: Theme.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                            maximumLineCount: 3
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 14
                            text: parent.page && !parent.page.onStartPage ? BrowserBackend.displayUrl(parent.page.url) : ""
                            color: Theme.muted
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                        }
                    }
                }
                IconButton {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 4
                    glyph: "close"
                    iconColor: Theme.muted
                    onClicked: window.closeTab(cell.index)
                }
            }
        }
    }

    SheetPage {
        anchors.fill: pages
        visible: window.sheet === "bookmarks"
        title: qsTr("Bookmarks")
        onCloseRequested: window.sheet = ""
        LinkList {
            anchors.fill: parent
            model: BrowserBackend.bookmarks
            removable: true
            emptyText: qsTr("Tap the star in the menu to bookmark a page.")
            onOpened: url => window.open(url)
            onRemoved: url => BrowserBackend.removeBookmark(url)
        }
    }

    SheetPage {
        anchors.fill: pages
        visible: window.sheet === "history"
        title: qsTr("History")
        onCloseRequested: window.sheet = ""
        headerActions: [
            IconButton { glyph: "trash"; enabled: BrowserBackend.history.length > 0; onClicked: BrowserBackend.clearHistory() }
        ]
        LinkList {
            anchors.fill: parent
            model: BrowserBackend.history
            emptyText: qsTr("Pages you visit will be listed here.")
            onOpened: url => window.open(url)
        }
    }

    SheetPage {
        anchors.fill: pages
        visible: window.sheet === "settings"
        title: qsTr("Settings")
        onCloseRequested: window.sheet = ""
        Flickable {
            anchors.fill: parent
            contentHeight: settingsColumn.implicitHeight + 24
            clip: true
            Column {
                id: settingsColumn
                width: parent.width
                Text {
                    x: 20; topPadding: 8; bottomPadding: 4
                    text: qsTr("Search engine"); color: Theme.muted; font.pixelSize: 14; font.weight: Font.DemiBold
                }
                Repeater {
                    model: BrowserBackend.searchEngines
                    delegate: MenuRow {
                        required property int index
                        required property string modelData
                        width: settingsColumn.width
                        label: modelData
                        glyph: index === BrowserBackend.searchEngine ? "check" : ""
                        onClicked: BrowserBackend.searchEngine = index
                    }
                }
                Text {
                    x: 20; topPadding: 18; bottomPadding: 4
                    text: qsTr("Blocking"); color: Theme.muted; font.pixelSize: 14; font.weight: Font.DemiBold
                }
                MenuRow {
                    width: parent.width; toggle: true
                    label: qsTr("Hide ads")
                    detail: qsTr("Hides ad boxes, and mutes and skips ads in YouTube videos.")
                    checked: BrowserBackend.blockAds
                    onToggled: BrowserBackend.blockAds = checked
                }
                MenuRow {
                    width: parent.width; toggle: true
                    label: qsTr("Skip sponsor segments")
                    detail: qsTr("In YouTube videos, with SponsorBlock's community timestamps. Only a short fragment of a hash of the video's ID is sent.")
                    checked: BrowserBackend.skipSponsors
                    onToggled: BrowserBackend.skipSponsors = checked
                }
                Text {
                    x: 20; topPadding: 18; bottomPadding: 4
                    text: qsTr("Your data"); color: Theme.muted; font.pixelSize: 14; font.weight: Font.DemiBold
                }
                MenuRow {
                    width: parent.width; glyph: "trash"
                    label: qsTr("Clear history and cookies")
                    detail: qsTr("Signs you out of websites. Bookmarks are kept.")
                    onClicked: {
                        BrowserBackend.clearHistory();
                        if (window.tab) window.tab.clearCookies();
                        window.sheet = "";
                    }
                }
            }
        }
    }

    SheetPage {
        anchors.fill: pages
        visible: window.sheet === "about"
        title: qsTr("About")
        onCloseRequested: window.sheet = ""
        Column {
            anchors.centerIn: parent
            width: parent.width - 48
            spacing: 14
            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                source: "qrc:/icons/spacepenguin-256.png"
                sourceSize: Qt.size(96, 96)
            }
            Text {
                width: parent.width; horizontalAlignment: Text.AlignHCenter
                text: "SpacePenguin " + BrowserBackend.version
                color: Theme.text; font.pixelSize: 22; font.weight: Font.Bold
            }
            Text {
                width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                color: Theme.muted; font.pixelSize: 15
                text: qsTr("A small, private browser. Pages are shown by your phone's own web engine. "
                           + "No account, no telemetry. Free software under the GNU GPL v3.")
            }
            Text {
                width: parent.width; horizontalAlignment: Text.AlignHCenter
                textFormat: Text.StyledText
                text: "<a href=\"https://github.com/developer1-vd/SpacePenguin\">github.com/developer1-vd/SpacePenguin</a>"
                linkColor: Theme.accent; font.pixelSize: 15
                onLinkActivated: link => window.open(link)
            }
        }
    }

    // ---- Toolbar, at the bottom where thumbs reach ---------------------------
    Rectangle {
        id: toolbar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 64
        color: Theme.bar

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        // Load progress along the top edge.
        Rectangle {
            visible: window.tab && window.tab.loading
            height: 2
            width: parent.width * (window.tab ? window.tab.progress / 100 : 0)
            color: Theme.accent
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 0

            IconButton {
                visible: !window.editing
                glyph: "back"
                enabled: window.tab && !window.tab.onStartPage
                onClicked: window.tab.goBack()
            }
            IconButton {
                visible: !window.editing
                glyph: "forward"
                enabled: window.tab && window.tab.canGoForward
                onClicked: window.tab.goForward()
            }

            TextField {
                id: addressField
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                Layout.leftMargin: 4
                Layout.rightMargin: 4
                leftPadding: 16
                rightPadding: 16
                color: Theme.text
                placeholderText: qsTr("Search or type an address")
                placeholderTextColor: Theme.muted
                font.pixelSize: 16
                selectByMouse: true
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                text: window.editing ? text : (window.tab && !window.tab.onStartPage
                                               ? BrowserBackend.displayUrl(window.tab.url) : "")
                background: Rectangle {
                    radius: height / 2
                    color: Theme.field
                    border.color: addressField.activeFocus ? Theme.accent : "transparent"
                    border.width: 2
                }
                onActiveFocusChanged: {
                    if (activeFocus) {
                        window.editing = true;
                        text = window.tab && !window.tab.onStartPage ? window.tab.url.toString() : "";
                        selectAll();
                    }
                }
                onAccepted: if (text.trim()) window.open(text)
                // Outside editing, show where the address starts, not where it ends.
                onTextChanged: if (!window.editing) cursorPosition = 0
                Keys.onEscapePressed: { window.editing = false; focus = false; }
            }

            IconButton {
                visible: window.editing
                glyph: "close"
                onClicked: { window.editing = false; addressField.focus = false; }
            }
            IconButton {
                visible: !window.editing && window.tab && !window.tab.onStartPage
                glyph: window.tab && window.tab.loading ? "close" : "reload"
                onClicked: window.tab.loading ? window.tab.stop() : window.tab.reload()
            }

            // Tab count in a rounded square.
            AbstractButton {
                id: tabsButton
                visible: !window.editing
                implicitWidth: Theme.touch
                implicitHeight: Theme.touch
                onClicked: window.sheet = window.sheet === "tabs" ? "" : "tabs"
                background: Rectangle { radius: width / 2; color: tabsButton.pressed ? Theme.pressed : "transparent" }
                contentItem: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        width: 22; height: 22; radius: 6
                        color: "transparent"
                        border.color: Theme.text; border.width: 1.8
                        Text {
                            anchors.centerIn: parent
                            text: tabModel.count > 99 ? ":)" : tabModel.count
                            color: Theme.text; font.pixelSize: 12; font.weight: Font.Bold
                        }
                    }
                }
            }
            IconButton {
                visible: !window.editing
                glyph: "menu"
                onClicked: window.sheet = window.sheet === "menu" ? "" : "menu"
            }
        }
    }

    // Leaving the address field (tapping elsewhere, or Back) ends editing.
    Connections {
        target: addressField
        function onActiveFocusChanged() {
            if (!addressField.activeFocus)
                window.editing = false;
        }
    }
}

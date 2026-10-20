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

    // Menus and buttons in the app's own look, the same on every phone.
    component ThemedMenu: Menu {
        topPadding: 6
        bottomPadding: 6
        background: Rectangle {
            implicitWidth: 250
            radius: 16
            color: Theme.surface
            border.color: Theme.border
        }
    }
    component ThemedMenuItem: MenuItem {
        id: entry
        implicitHeight: 48
        leftPadding: 18
        rightPadding: 18
        contentItem: Text {
            text: entry.text
            color: entry.enabled ? Theme.text : Theme.muted
            font.pixelSize: 16
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            x: 6
            width: entry.width - 12
            height: entry.height
            radius: 12
            color: entry.highlighted || entry.pressed ? Theme.pressed : "transparent"
        }
    }
    component ThemedSeparator: MenuSeparator {
        topPadding: 4
        bottomPadding: 4
        contentItem: Rectangle { implicitHeight: 1; color: Theme.border }
    }
    component PillButton: AbstractButton {
        id: pill
        property bool primary: false
        implicitHeight: 44
        leftPadding: 20
        rightPadding: 20
        contentItem: Text {
            text: pill.text
            color: pill.primary ? Theme.accentText : Theme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: height / 2
            color: pill.primary ? (pill.pressed ? Qt.darker(Theme.accent, 1.15) : Theme.accent)
                                : (pill.pressed ? Theme.pressed : "transparent")
        }
    }

    // Tab groups, as on the desktop: { id, title, color, collapsed }. A tab's group is
    // its "group" role, 0 for none. Tabs are never moved in the model (that would
    // reload their pages); the tabs screen shows each group's tabs together instead.
    property var groups: []
    property int nextGroupId: 1
    // Bumped when a tab's group changes, which a ListModel does not announce to bindings.
    property int tabRevision: 0

    function newTab(address, isPrivate) {
        tabModel.append({ first: address || "", group: 0, isPrivate: !!isPrivate });
        current = tabModel.count - 1;
        sheet = "";
        editing = false;
    }
    function closeTab(index) {
        tabModel.remove(index);
        if (tabModel.count === 0)
            newTab("");
        current = Math.min(current, tabModel.count - 1);
        removeEmptyGroups();
    }

    function groupById(id) {
        return groups.find(g => g.id === id) || null;
    }
    function createGroup(title, color) {
        const group = { id: nextGroupId++, title: title, color: color, collapsed: false };
        groups = groups.concat([group]);
        return group.id;
    }
    function updateGroup(id, changes) {
        groups = groups.map(g => g.id === id ? Object.assign({}, g, changes) : g);
    }
    function setTabGroup(index, id) {
        tabModel.setProperty(index, "group", id);
        tabRevision++;
        removeEmptyGroups();
    }
    function removeEmptyGroups() {
        const used = {};
        for (let i = 0; i < tabModel.count; ++i)
            used[tabModel.get(i).group] = true;
        if (groups.some(g => !used[g.id]))
            groups = groups.filter(g => used[g.id]);
        tabRevision++;
    }
    function ungroup(id) {
        for (let i = 0; i < tabModel.count; ++i) {
            if (tabModel.get(i).group === id)
                tabModel.setProperty(i, "group", 0);
        }
        removeEmptyGroups();
    }
    function closeGroup(id) {
        for (let i = tabModel.count - 1; i >= 0; --i) {
            if (tabModel.get(i).group === id)
                closeTab(i);
        }
    }
    function newTabInGroup(id) {
        newTab("");
        setTabGroup(tabModel.count - 1, id);
    }
    function nextFreeColor() {
        for (let i = 1; i <= 9; ++i) {
            if (!groups.some(g => g.color === i % 9))
                return i % 9;
        }
        return 1;
    }

    // The tabs screen's sections: runs of tabs outside groups, and each group with
    // all its tabs, where the group's first tab is.
    readonly property var tabSections: {
        tabRevision;
        const sections = [];
        const placed = {};
        for (let i = 0; i < tabModel.count; ++i) {
            const group = tabModel.get(i).group;
            if (group && placed[group] !== undefined) {
                sections[placed[group]].tabs.push(i);
            } else if (!group && sections.length && sections[sections.length - 1].group === 0) {
                sections[sections.length - 1].tabs.push(i);
            } else {
                if (group)
                    placed[group] = sections.length;
                sections.push({ group: group, tabs: [i] });
            }
        }
        return sections;
    }
    function open(text) {
        editing = false;
        sheet = "";
        if (tab)
            tab.load(BrowserBackend.resolve(text));
    }

    // The tabs and groups, kept when the app goes to the background (where the
    // phone may close it) and when it closes, and reopened at start.
    function saveSession() {
        const tabs = [];
        let currentIndex = 0;
        for (let i = 0; i < tabModel.count; ++i) {
            const entry = tabModel.get(i);
            const page = tabRepeater.itemAt(i);
            if (entry.isPrivate)
                continue;
            if (i === current)
                currentIndex = tabs.length;
            tabs.push({ url: page && !page.onStartPage ? page.url.toString() : "", group: entry.group });
        }
        BrowserBackend.saveSession({ tabs: tabs, groups: groups, current: currentIndex });
    }
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state !== Qt.ApplicationActive)
                window.saveSession();
        }
    }
    Component.onCompleted: {
        const saved = BrowserBackend.savedSession();
        const tabs = saved.tabs || [];
        if (!tabs.length) {
            newTab("");
            return;
        }
        groups = (saved.groups || []).filter(g => tabs.some(t => t.group === g.id));
        nextGroupId = groups.reduce((top, g) => Math.max(top, g.id + 1), 1);
        for (const saved_tab of tabs)
            tabModel.append({ first: saved_tab.url || "", group: saved_tab.group || 0, isPrivate: false });
        current = Math.min(saved.current || 0, tabModel.count - 1);
        tabRevision++;
    }

    // Android's Back button closes what is open, then goes back, then leaves.
    onClosing: close => {
        if (sheet !== "") { sheet = ""; close.accepted = false; }
        else if (editing) { editing = false; close.accepted = false; }
        else if (tab && !tab.onStartPage) { tab.goBack(); close.accepted = false; }
        else saveSession();
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
                required isPrivate
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
                MenuRow { width: parent.width; glyph: "shield"; label: qsTr("New private tab"); onClicked: window.newTab("", true) }
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
        Flickable {
            id: tabsFlick
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            contentHeight: sectionsColumn.implicitHeight + 24
            clip: true
            readonly property int columns: Math.max(2, Math.floor(width / 180))
            readonly property real cardWidth: width / columns

            Column {
                id: sectionsColumn
                width: parent.width
                Repeater {
                    model: window.tabSections
                    delegate: Column {
                        id: section
                        required property var modelData
                        readonly property var group: window.groupById(modelData.group)
                        width: sectionsColumn.width
                        bottomPadding: group ? 10 : 0

                        // A group's header: tap to collapse, ⋮ for the rest.
                        AbstractButton {
                            id: header
                            visible: section.group !== null
                            width: parent.width
                            height: visible ? 52 : 0
                            onClicked: window.updateGroup(section.group.id, { collapsed: !section.group.collapsed })
                            onPressAndHold: groupMenu.openFor(section.group.id)
                            background: Rectangle { radius: 14; color: header.pressed ? Theme.pressed : "transparent" }
                            contentItem: RowLayout {
                                spacing: 10
                                Rectangle {
                                    Layout.leftMargin: 8
                                    width: 12; height: 12; radius: 6
                                    color: section.group ? Theme.groupColor(section.group.color) : "transparent"
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: !section.group ? ""
                                          : (section.group.title || qsTr("Group"))
                                            + (section.group.collapsed ? "  ·  " + section.modelData.tabs.length : "")
                                    color: section.group && section.group.title ? Theme.text : Theme.muted
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Icon {
                                    width: 22; height: 22
                                    name: section.group && section.group.collapsed ? "chevron-right" : "chevron-down"
                                    color: Theme.muted
                                }
                                IconButton {
                                    glyph: "menu"
                                    iconColor: Theme.muted
                                    onClicked: groupMenu.openFor(section.group.id)
                                }
                            }
                        }

                        Flow {
                            width: parent.width
                            visible: !section.group || !section.group.collapsed
                            Repeater {
                                model: section.modelData.tabs
                                delegate: Item {
                                    id: cell
                                    required property int modelData
                                    readonly property int tabIndex: modelData
                                    readonly property var page: tabRepeater.itemAt(tabIndex)
                                    width: tabsFlick.cardWidth
                                    height: 120
                                    AbstractButton {
                                        id: card
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        onClicked: { window.current = cell.tabIndex; window.sheet = ""; }
                                        onPressAndHold: tabMenu.openFor(cell.tabIndex)
                                        background: Rectangle {
                                            radius: Theme.radius
                                            color: card.pressed ? Theme.pressed
                                                   : cell.page && cell.page.isPrivate ? Theme.privateSurface : Theme.surface
                                            border.color: cell.tabIndex === window.current ? Theme.accent : Theme.border
                                            border.width: cell.tabIndex === window.current ? 2 : 1
                                            // A group's tabs carry its colour along their top.
                                            Rectangle {
                                                visible: section.group !== null
                                                anchors.top: parent.top
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                anchors.topMargin: 6
                                                width: parent.width - 2 * Theme.radius
                                                height: 3
                                                radius: 1.5
                                                color: section.group ? Theme.groupColor(section.group.color) : "transparent"
                                            }
                                        }
                                        contentItem: Item {
                                            Text {
                                                anchors.fill: parent
                                                anchors.margins: 14
                                                anchors.rightMargin: 40
                                                text: cell.page ? cell.page.title : ""
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
                                                text: (cell.page && cell.page.isPrivate ? qsTr("Private · ") : "")
                                                      + (cell.page && !cell.page.onStartPage ? BrowserBackend.displayUrl(cell.page.url) : "")
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
                                        onClicked: window.closeTab(cell.tabIndex)
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    width: parent.width
                    topPadding: 12
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    visible: window.groups.length === 0 && tabModel.count > 1
                    text: qsTr("Touch and hold a tab to put it in a group.")
                    color: Theme.muted
                    font.pixelSize: 13
                }
            }
        }
    }

    // ---- Tab groups: menus and the name-and-colour editor ----------------------
    ThemedMenu {
        id: tabMenu
        property int tabIndex: -1
        readonly property int tabGroup: tabIndex >= 0 && tabIndex < tabModel.count
                                        ? (window.tabRevision, tabModel.get(tabIndex).group) : 0
        function openFor(index) {
            tabIndex = index;
            popup();
        }
        ThemedMenuItem { text: qsTr("Add to new group"); onTriggered: groupEditor.openFor(0, tabMenu.tabIndex) }
        Instantiator {
            model: window.groups
            delegate: ThemedMenuItem {
                required property var modelData
                visible: modelData.id !== tabMenu.tabGroup
                height: visible ? implicitHeight : 0
                text: qsTr("Add to “%1”").arg(modelData.title || qsTr("Group"))
                onTriggered: window.setTabGroup(tabMenu.tabIndex, modelData.id)
            }
            onObjectAdded: (index, item) => tabMenu.insertItem(index + 1, item)
            onObjectRemoved: (index, item) => tabMenu.removeItem(item)
        }
        ThemedMenuItem {
            text: qsTr("Remove from group")
            enabled: tabMenu.tabGroup !== 0
            onTriggered: window.setTabGroup(tabMenu.tabIndex, 0)
        }
        ThemedSeparator {}
        ThemedMenuItem { text: qsTr("Close tab"); onTriggered: window.closeTab(tabMenu.tabIndex) }
    }

    ThemedMenu {
        id: groupMenu
        property int groupId: 0
        function openFor(id) {
            groupId = id;
            popup();
        }
        ThemedMenuItem { text: qsTr("Name and colour…"); onTriggered: groupEditor.openFor(groupMenu.groupId, -1) }
        ThemedMenuItem { text: qsTr("New tab in group"); onTriggered: window.newTabInGroup(groupMenu.groupId) }
        ThemedMenuItem { text: qsTr("Ungroup"); onTriggered: window.ungroup(groupMenu.groupId) }
        ThemedSeparator {}
        ThemedMenuItem { text: qsTr("Close group"); onTriggered: window.closeGroup(groupMenu.groupId) }
    }

    Popup {
        id: groupEditor
        // groupId 0: a new group for the tab at tabIndex.
        property int groupId: 0
        property int tabIndex: -1
        property int color: 1
        function openFor(id, index) {
            groupId = id;
            tabIndex = index;
            const group = window.groupById(id);
            nameField.text = group ? group.title : "";
            color = group ? group.color : window.nextFreeColor();
            open();
            nameField.forceActiveFocus();
        }
        function accept() {
            const title = nameField.text.trim();
            if (groupId === 0)
                window.setTabGroup(tabIndex, window.createGroup(title, color));
            else
                window.updateGroup(groupId, { title: title, color: color });
            close();
        }
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(window.width - 40, 380)
        modal: true
        padding: 20
        background: Rectangle { radius: Theme.radius; color: Theme.surface; border.color: Theme.border }
        contentItem: Column {
            spacing: 16
            Text {
                text: groupEditor.groupId === 0 ? qsTr("New group") : qsTr("Edit group")
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            TextField {
                id: nameField
                width: parent.width
                height: 48
                leftPadding: 16
                color: Theme.text
                placeholderText: qsTr("Name this group")
                placeholderTextColor: Theme.muted
                font.pixelSize: 16
                background: Rectangle {
                    radius: height / 2
                    color: Theme.field
                    border.color: nameField.activeFocus ? Theme.accent : "transparent"
                    border.width: 2
                }
                onAccepted: groupEditor.accept()
            }
            Flow {
                width: parent.width
                spacing: 6
                Repeater {
                    model: Theme.groupColorNames
                    delegate: AbstractButton {
                        id: swatch
                        required property int index
                        required property string modelData
                        width: 32; height: 32
                        Accessible.name: modelData
                        onClicked: groupEditor.color = index
                        background: Rectangle {
                            radius: width / 2
                            color: Theme.groupColor(swatch.index)
                            border.color: Theme.text
                            border.width: groupEditor.color === swatch.index ? 3 : 0
                        }
                    }
                }
            }
            RowLayout {
                width: parent.width
                Item { Layout.fillWidth: true }
                spacing: 8
                PillButton { text: qsTr("Cancel"); onClicked: groupEditor.close() }
                PillButton { text: qsTr("Done"); primary: true; onClicked: groupEditor.accept() }
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

import QtQuick
import QtWebView
import SpacePenguin

// One tab: the start page until something is opened, then a web view.
Item {
    id: tab

    // The web view is a native view drawn above everything else on Android and
    // iOS, so it is shown only while nothing should cover it.
    property bool pageVisible: true
    // A private tab is not recorded in history and not reopened with the app.
    property bool isPrivate: false

    property bool onStartPage: true
    readonly property url url: onStartPage ? "" : view.url
    readonly property string title: onStartPage ? qsTr("New tab")
                                                : (view.title || BrowserBackend.displayUrl(view.url))
    readonly property bool loading: !onStartPage && view.loading
    readonly property int progress: view.loadProgress
    readonly property bool canGoBack: !onStartPage && view.canGoBack
    readonly property bool canGoForward: !onStartPage && view.canGoForward

    signal openRequested(string text)

    function load(address) {
        onStartPage = false;
        view.url = address;
    }
    function goBack() {
        if (view.canGoBack)
            view.goBack();
        else
            onStartPage = true;
    }
    function goForward() { view.goForward(); }
    function reload() { view.reload(); }
    function stop() { view.stop(); }
    function showStartPage() { onStartPage = true; }
    function clearCookies() { view.deleteAllCookies(); }

    function runPageScripts() {
        const script = BrowserBackend.pageScript(view.url);
        if (script)
            view.runJavaScript(script);
        BrowserBackend.lookUpSponsors(view.url);
    }

    StartPage {
        anchors.fill: parent
        visible: tab.onStartPage
        onOpenRequested: text => tab.openRequested(text)
    }

    // Said plainly: the phone's web view cannot keep a tab's cookies apart.
    Text {
        visible: tab.onStartPage && tab.isPrivate
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: Theme.muted
        font.pixelSize: 13
        text: qsTr("Private tab: what you visit here is not saved to history, and the tab is not "
                   + "reopened with the app. Sites still share cookies with your other tabs; "
                   + "Settings › Clear history and cookies removes them.")
    }

    WebView {
        id: view
        anchors.fill: parent
        visible: tab.visible && tab.pageVisible && !tab.onStartPage

        onLoadingChanged: request => {
            if (request.status === WebView.LoadSucceededStatus) {
                if (!tab.isPrivate)
                    BrowserBackend.recordVisit(view.title, view.url);
                tab.runPageScripts();
            }
        }
        // YouTube changes videos without loading a new page.
        onUrlChanged: if (!loading) tab.runPageScripts()
    }

    Connections {
        target: BrowserBackend
        function onSponsorScriptReady(address, script) {
            if (address.toString() === view.url.toString())
                view.runJavaScript(script);
        }
    }
}

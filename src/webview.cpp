#include "webview.h"

#include "browserpage.h"

namespace spacepenguin {

WebView::WebView(QWebEngineProfile *profile, QWidget *parent)
    : QWebEngineView(parent)
    , m_page(new BrowserPage(profile, this))
{
    setPage(m_page);

    connect(this, &QWebEngineView::loadStarted, this, [this] {
        m_loading = true;
        m_failed = false;
        m_progress = 0;
    });
    connect(this, &QWebEngineView::loadProgress, this, [this](int progress) {
        m_progress = progress;
    });
    connect(this, &QWebEngineView::loadFinished, this, [this](bool ok) {
        m_loading = false;
        m_failed = !ok;
        m_progress = 100;
    });
}

QWebEngineView *WebView::createWindow(QWebEnginePage::WebWindowType type)
{
    // Pop-up windows and dialogs stay refused (JavascriptCanOpenWindows is off);
    // links the user opens in a new tab or window become tabs in this window.
    if (!m_tabFactory)
        return nullptr;

    switch (type) {
    case QWebEnginePage::WebBrowserTab:
    case QWebEnginePage::WebBrowserWindow:
        return m_tabFactory(false);
    case QWebEnginePage::WebBrowserBackgroundTab:
        return m_tabFactory(true);
    case QWebEnginePage::WebDialog:
        break;
    }
    return nullptr;
}

}

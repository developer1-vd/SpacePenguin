#include "webview.h"

#include "browserpage.h"
#include "preferences.h"
#include "roles.h"

#include <QContextMenuEvent>
#include <QMenu>

namespace spacepenguin {

void WebView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);
    // Qt's own Inspect needs developer tools already attached; the window attaches them.
    menu->removeAction(pageAction(QWebEnginePage::InspectElement));
    if (Preferences::instance()->hasRole(Roles::key(Roles::Role::Developer))) {
        menu->addSeparator();
        QAction *inspect = menu->addAction(tr("Inspect"));
        connect(inspect, &QAction::triggered, this, &WebView::inspectRequested);
    }
    menu->popup(event->globalPos());
}

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

WebView *WebView::openDialog()
{
    // A site's own small window, such as a sign-in box: it stays over this window
    // and closes when the site is done with it. Its title names the site, since it
    // has no address bar.
    auto *dialog = new WebView(page()->profile(), this);
    dialog->setWindowFlags(Qt::Dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setTabFactory(m_tabFactory);
    const auto retitle = [dialog] {
        const QString host = dialog->url().host();
        dialog->setWindowTitle(host.isEmpty() ? dialog->title()
                                              : QStringLiteral("%1 — %2").arg(host, dialog->title()));
    };
    connect(dialog, &QWebEngineView::titleChanged, dialog, retitle);
    connect(dialog, &QWebEngineView::urlChanged, dialog, retitle);
    connect(dialog->page(), &QWebEnginePage::windowCloseRequested, dialog, &QWidget::close);
    connect(dialog->page(), &QWebEnginePage::geometryChangeRequested, dialog, [dialog](const QRect &geometry) {
        if (geometry.width() > 0 && geometry.height() > 0)
            dialog->resize(geometry.size());
    });
    dialog->resize(500, 600);
    dialog->show();
    return dialog;
}

QWebEngineView *WebView::createWindow(QWebEnginePage::WebWindowType type)
{
    // Pop-ups the user did not ask for stay refused (JavascriptCanOpenWindows is off);
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
        return openDialog();
    }
    return nullptr;
}

}

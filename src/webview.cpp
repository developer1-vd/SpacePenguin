#include "webview.h"

#include "browserpage.h"
#include "preferences.h"
#include "roles.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QTimer>

namespace spacepenguin {

QString WebView::crashedPage(const QUrl &url, bool outOfMemory)
{
    const QString title = outOfMemory ? tr("This tab was stopped") : tr("This tab crashed");
    const QString text = outOfMemory
        ? tr("The computer ran low on memory, so the page was stopped to keep everything else running. "
             "Closing other tabs may help.")
        : tr("Something went wrong while showing this page. The rest of the browser is fine.");
    return QStringLiteral(
               "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"color-scheme\" "
               "content=\"light dark\"><title>%1</title><style>body{font:16px/1.5 system-ui,sans-serif;"
               "display:grid;place-items:center;min-height:90vh;margin:0;text-align:center}main{max-width:32rem;"
               "padding:2rem}a.button{display:inline-block;margin-top:1rem;padding:.5rem 1.4rem;border-radius:999px;"
               "background:#3166e0;color:#fff;text-decoration:none}</style></head><body><main>"
               "<div style=\"font-size:3rem\">%2</div><h1>%1</h1><p>%3</p><a class=\"button\" href=\"%4\">%5</a>"
               "</main></body></html>")
        .arg(title.toHtmlEscaped(), outOfMemory ? QStringLiteral("🧠") : QStringLiteral("💥"), text.toHtmlEscaped(),
             url.toString(QUrl::FullyEncoded).toHtmlEscaped(), tr("Reload").toHtmlEscaped());
}

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
    // A page whose process died (a crash, or the system running out of memory) says
    // so, with a Reload button, instead of staying blank.
    connect(m_page, &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus status, int) {
                if (status == QWebEnginePage::NormalTerminationStatus)
                    return;
                const QUrl address = url();
                const bool outOfMemory = status == QWebEnginePage::KilledTerminationStatus;
                QTimer::singleShot(0, this, [this, address, outOfMemory] {
                    setHtml(crashedPage(address, outOfMemory), address);
                });
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

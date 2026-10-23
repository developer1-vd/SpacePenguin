#include "appwindow.h"

#include "browserpage.h"
#include "preferences.h"
#include "sitepermissions.h"
#include "webview.h"

#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QMessageBox>
#include <QPixmap>
#include <QSettings>
#include <QShortcut>
#include <QWebEngineFullScreenRequest>
#include <QWebEnginePermission>

namespace spacepenguin {
namespace {

QString geometryKey(const QString &id)
{
    return QStringLiteral("webapps/%1/geometry").arg(id);
}

} // namespace

AppWindow *AppWindow::open(const QString &id, QWebEngineProfile *profile, TabOpener openTab)
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        auto *window = qobject_cast<AppWindow *>(widget);
        if (window && window->appId() == id) {
            if (window->isMinimized())
                window->showNormal();
            window->show();
            window->raise();
            window->activateWindow();
            return window;
        }
    }
    const WebApp app = WebApps::instance()->app(id);
    if (!app.isValid() || !profile)
        return nullptr;
    auto *window = new AppWindow(app, profile, std::move(openTab));
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
    window->raise();
    window->activateWindow();
    return window;
}

AppWindow::AppWindow(const WebApp &app, QWebEngineProfile *profile, TabOpener openTab, QWidget *parent)
    : QMainWindow(parent)
    , m_app(app)
    , m_openTab(std::move(openTab))
{
    setObjectName(QStringLiteral("appWindow"));
    setWindowTitle(app.name);
    const QImage icon = WebApps::instance()->icon(app.id);
    if (!icon.isNull())
        setWindowIcon(QIcon(QPixmap::fromImage(icon)));
    resize(1100, 760);
    restoreGeometry(QSettings().value(geometryKey(app.id)).toByteArray());

    m_view = new WebView(profile, this);
    setCentralWidget(m_view);
    m_view->setZoomFactor(Preferences::instance()->pageZoom() / 100.0);

    // Links the person follows to other sites open in the browser; redirects and the
    // site's own moves (such as signing in elsewhere and coming back) stay here.
    m_view->browserPage()->setNavigationFilter([this](const QUrl &url, QWebEnginePage::NavigationType type) {
        const bool web = url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https");
        if (type == QWebEnginePage::NavigationTypeLinkClicked && web && !m_app.contains(url)) {
            openInBrowser(url);
            return false;
        }
        return true;
    });
    // A page the site opens in a new tab or window is the browser's.
    m_view->setTabFactory([this](bool background) -> WebView * {
        return m_openTab ? m_openTab(background) : nullptr;
    });

    connect(m_view, &QWebEngineView::titleChanged, this, [this](const QString &title) {
        // The page's title, followed by the app's name unless the title has it already.
        setWindowTitle(title.isEmpty() ? m_app.name
                       : title.contains(m_app.name, Qt::CaseInsensitive)
                           ? title
                           : QStringLiteral("%1 — %2").arg(title, m_app.name));
    });
    connect(m_view->page(), &QWebEnginePage::fullScreenRequested, this,
            [this](QWebEngineFullScreenRequest request) {
                request.accept();
                if (request.toggleOn())
                    showFullScreen();
                else
                    showNormal();
            });
    connect(m_view->page(), &QWebEnginePage::windowCloseRequested, this, &QWidget::close);
    connect(m_view->page(), &QWebEnginePage::permissionRequested, this,
            [this](QWebEnginePermission permission) {
                const QString site = SitePermissions::siteName(permission.origin());
                const auto answer = QMessageBox::question(
                    this, m_app.name,
                    tr("%1 wants to %2.").arg(site, SitePermissions::request(permission.permissionType())),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer == QMessageBox::Yes)
                    permission.grant();
                else
                    permission.deny();
            });

    const auto shortcut = [this](const QKeySequence &keys, auto slot) {
        auto *action = new QShortcut(keys, this);
        connect(action, &QShortcut::activated, this, slot);
    };
    shortcut(QKeySequence(Qt::ALT | Qt::Key_Left), [this] { m_view->back(); });
    shortcut(QKeySequence(Qt::ALT | Qt::Key_Right), [this] { m_view->forward(); });
    shortcut(QKeySequence::Back, [this] { m_view->back(); });
    shortcut(QKeySequence::Forward, [this] { m_view->forward(); });
    shortcut(QKeySequence(Qt::Key_F5), [this] { m_view->reload(); });
    shortcut(QKeySequence::Refresh, [this] { m_view->reload(); });
    shortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B), [this] { openInBrowser(m_view->url()); });
    shortcut(QKeySequence::ZoomIn, [this] { setZoom(m_view->zoomFactor() + 0.1); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), [this] { setZoom(m_view->zoomFactor() + 0.1); });
    shortcut(QKeySequence::ZoomOut, [this] { setZoom(m_view->zoomFactor() - 0.1); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_0), [this] { setZoom(Preferences::instance()->pageZoom() / 100.0); });
    shortcut(QKeySequence(Qt::Key_F11), [this] { isFullScreen() ? showNormal() : showFullScreen(); });
    shortcut(QKeySequence::Close, [this] { close(); });

    // Removed in Settings › Apps: the window goes with it.
    connect(WebApps::instance(), &WebApps::changed, this, [this] {
        if (!WebApps::instance()->app(m_app.id).isValid())
            close();
    });

    m_view->load(app.startUrl);
}

void AppWindow::closeEvent(QCloseEvent *event)
{
    QSettings().setValue(geometryKey(m_app.id), saveGeometry());
    QMainWindow::closeEvent(event);
}

void AppWindow::openInBrowser(const QUrl &url)
{
    if (!m_openTab)
        return;
    if (WebView *tab = m_openTab(false))
        tab->load(url);
}

void AppWindow::setZoom(qreal factor)
{
    m_view->setZoomFactor(qBound(0.25, factor, 5.0));
}

}

#include "browserwindow.h"

#include "browserpage.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QWebEngineHistory>
#include <QWebEngineView>

namespace spacepenguin {
namespace {

constexpr qreal kMinimumZoom = 0.5;
constexpr qreal kMaximumZoom = 2.0;
constexpr qreal kZoomStep = 0.1;
constexpr int kMaximumRestoredTabs = 32;

QIcon themeIcon(const QString &name, QStyle::StandardPixmap fallback)
{
    const QIcon icon = QIcon::fromTheme(name);
    if (!icon.isNull())
        return icon;
    return QApplication::style()->standardIcon(fallback);
}

} // namespace

BrowserWindow::BrowserWindow(QWebEngineProfile *profile, QWidget *parent)
    : QMainWindow(parent)
    , m_profile(profile)
{
    setWindowTitle(QStringLiteral("SpacePenguin"));
    resize(1100, 720);

    createActions();
    createToolBar();
    createMenus();
    createTabWidget();

    statusBar()->addPermanentWidget(m_securityLabel);
    statusBar()->addPermanentWidget(m_progressBar, 1);

    restoreSession();
}

BrowserWindow::~BrowserWindow() = default;

void BrowserWindow::createActions()
{
    m_newTabAction = new QAction(themeIcon(QStringLiteral("tab-new"),
                                           QStyle::SP_FileDialogNewFolder), tr("New Tab"), this);
    m_newTabAction->setShortcut(QKeySequence::New);
    connect(m_newTabAction, &QAction::triggered, this, [this] { newTab(m_resolver.homeUrl()); });

    m_closeTabAction = new QAction(tr("Close Tab"), this);
    m_closeTabAction->setShortcut(QKeySequence::Close);
    connect(m_closeTabAction, &QAction::triggered, this,
            [this] { closeTab(m_tabs->currentIndex()); });

    m_backAction = new QAction(themeIcon(QStringLiteral("go-previous"),
                                         QStyle::SP_ArrowBack), tr("Back"), this);
    m_backAction->setShortcut(QKeySequence::Back);
    connect(m_backAction, &QAction::triggered, this,
            [this] { if (QWebEngineView *view = currentView()) view->back(); });

    m_forwardAction = new QAction(themeIcon(QStringLiteral("go-next"),
                                            QStyle::SP_ArrowForward), tr("Forward"), this);
    m_forwardAction->setShortcut(QKeySequence::Forward);
    connect(m_forwardAction, &QAction::triggered, this,
            [this] { if (QWebEngineView *view = currentView()) view->forward(); });

    m_reloadAction = new QAction(themeIcon(QStringLiteral("view-refresh"),
                                            QStyle::SP_BrowserReload), tr("Reload"), this);
    m_reloadAction->setShortcut(QKeySequence::Refresh);
    connect(m_reloadAction, &QAction::triggered, this,
            [this] { if (QWebEngineView *view = currentView()) view->reload(); });

    m_stopAction = new QAction(themeIcon(QStringLiteral("process-stop"),
                                         QStyle::SP_BrowserStop), tr("Stop"), this);
    m_stopAction->setEnabled(false);
    connect(m_stopAction, &QAction::triggered, this,
            [this] { if (QWebEngineView *view = currentView()) view->stop(); });

    m_homeAction = new QAction(themeIcon(QStringLiteral("go-home"),
                                         QStyle::SP_DirHomeIcon), tr("Home"), this);
    connect(m_homeAction, &QAction::triggered, this, &BrowserWindow::goHome);

    m_zoomInAction = new QAction(tr("Zoom In"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this,
            [this] { setZoom(currentZoom() + kZoomStep); });

    m_zoomOutAction = new QAction(tr("Zoom Out"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this,
            [this] { setZoom(currentZoom() - kZoomStep); });

    m_zoomResetAction = new QAction(tr("Reset Zoom"), this);
    m_zoomResetAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_zoomResetAction, &QAction::triggered, this, [this] { setZoom(1.0); });
}

void BrowserWindow::createToolBar()
{
    auto *toolBar = addToolBar(tr("Navigation"));
    toolBar->setMovable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    toolBar->addAction(m_backAction);
    toolBar->addAction(m_forwardAction);
    toolBar->addAction(m_reloadAction);
    toolBar->addAction(m_stopAction);
    toolBar->addAction(m_homeAction);

    m_omnibox = new QLineEdit;
    m_omnibox->setPlaceholderText(tr("Search or enter address"));
    m_omnibox->setClearButtonEnabled(true);
    m_omnibox->setText(m_resolver.homeUrl().toDisplayString());
    m_omnibox->installEventFilter(this);
    connect(m_omnibox, &QLineEdit::returnPressed, this, &BrowserWindow::onOmniboxReturnPressed);
    connect(m_omnibox, &QLineEdit::textEdited, this, [this] { m_omniboxEdited = true; });

    auto *focusAction = new QAction(tr("Focus Address Bar"), this);
    focusAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(focusAction, &QAction::triggered, this, [this] {
        m_omnibox->setFocus();
        m_omnibox->selectAll();
    });
    addAction(focusAction);

    toolBar->addWidget(m_omnibox);
}

void BrowserWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_newTabAction);
    fileMenu->addAction(m_closeTabAction);
    fileMenu->addSeparator();

    QAction *newWindowAction = fileMenu->addAction(tr("New Window"));
    newWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(newWindowAction, &QAction::triggered, this, [this] {
        auto *window = new BrowserWindow(m_profile);
        window->newTab(m_resolver.homeUrl());
        window->show();
    });

    QAction *quitAction = fileMenu->addAction(tr("Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    quitAction->setMenuRole(QAction::QuitRole);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_reloadAction);
    viewMenu->addAction(m_stopAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_zoomResetAction);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("About SpacePenguin"));
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("About SpacePenguin"),
                           tr("<h3>SpacePenguin %1</h3>"
                              "<p>A secure, lightweight, and usable browser written in Qt.</p>"
                              "<p>Pre-alpha software. Expect breakage.</p>")
                               .arg(QApplication::applicationVersion()));
    });
}

void BrowserWindow::createTabWidget()
{
    m_tabs = new QTabWidget;
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->setElideMode(Qt::ElideMiddle);
    setCentralWidget(m_tabs);

    auto *newTabButton = new QToolButton;
    newTabButton->setAutoRaise(true);
    newTabButton->setIcon(m_newTabAction->icon());
    newTabButton->setToolTip(tr("New Tab"));
    newTabButton->setFixedSize(20, 20);
    connect(newTabButton, &QToolButton::clicked, this, [this] { newTab(m_resolver.homeUrl()); });
    m_tabs->setCornerWidget(newTabButton, Qt::TopRightCorner);

    connect(m_tabs, &QTabWidget::currentChanged, this, &BrowserWindow::onCurrentTabChanged);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &BrowserWindow::onTabCloseRequested);

    m_progressBar = new QProgressBar;
    m_progressBar->setMaximumWidth(180);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();

    m_securityLabel = new QLabel;
    m_securityLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
}

void BrowserWindow::connectView(QWebEngineView *view)
{
    connect(view, &QWebEngineView::titleChanged, this, &BrowserWindow::onTitleChanged);
    connect(view, &QWebEngineView::urlChanged, this, &BrowserWindow::onUrlChanged);
    connect(view, &QWebEngineView::loadStarted, this, &BrowserWindow::onLoadStarted);
    connect(view, &QWebEngineView::loadProgress, this, &BrowserWindow::onLoadProgress);
    connect(view, &QWebEngineView::loadFinished, this, &BrowserWindow::onLoadFinished);
}

void BrowserWindow::newTab(const QUrl &url)
{
    auto *view = new QWebEngineView;
    view->setPage(new BrowserPage(m_profile, view));
    view->setZoomFactor(1.0);
    connectView(view);

    m_tabs->addTab(view, tr("New Tab"));
    m_tabs->setCurrentWidget(view);
    view->setUrl(url.isEmpty() ? m_resolver.homeUrl() : url);
}

void BrowserWindow::openInNewTab(const QUrl &url)
{
    newTab(url);
}

void BrowserWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabs->count())
        return;

    QWidget *page = m_tabs->widget(index);
    m_tabs->removeTab(index);
    page->deleteLater();

    if (m_tabs->count() == 0)
        close();
}

void BrowserWindow::onTabCloseRequested(int index)
{
    closeTab(index);
}

void BrowserWindow::goHome()
{
    if (QWebEngineView *view = currentView())
        view->setUrl(m_resolver.homeUrl());
}

void BrowserWindow::navigate(const QString &text)
{
    if (QWebEngineView *view = currentView())
        view->setUrl(m_resolver.resolve(text));
}

void BrowserWindow::onOmniboxReturnPressed()
{
    navigate(m_omnibox->text());
    m_omniboxEdited = false;
    m_omnibox->setFocus();
}

void BrowserWindow::syncOmnibox(const QUrl &url)
{
    if (!m_omnibox->hasFocus() || !m_omniboxEdited)
        m_omnibox->setText(url.toDisplayString());
    m_omniboxEdited = false;
}

bool BrowserWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_omnibox && event->type() == QEvent::FocusOut)
        m_omniboxEdited = false;
    return QMainWindow::eventFilter(watched, event);
}

void BrowserWindow::onCurrentTabChanged(int index)
{
    Q_UNUSED(index)
    QWebEngineView *view = currentView();
    if (!view)
        return;

    m_tabs->setTabToolTip(m_tabs->currentIndex(), view->url().toDisplayString());
    syncOmnibox(view->url());

    updateNavigationState();
    updateWindowTitle();
    updateSecurityIndicator(view->url());
}

void BrowserWindow::onUrlChanged(const QUrl &url)
{
    QWebEngineView *view = currentView();
    if (!view || view->url() != url)
        return;

    syncOmnibox(url);

    m_tabs->setTabText(m_tabs->currentIndex(), url.host().isEmpty() ? tr("New Tab") : url.host());
    m_tabs->setTabToolTip(m_tabs->currentIndex(), url.toDisplayString());

    updateSecurityIndicator(url);
    updateNavigationState();
    updateWindowTitle();
}

void BrowserWindow::onTitleChanged(const QString &title)
{
    Q_UNUSED(title)
    updateWindowTitle();
}

void BrowserWindow::onLoadStarted()
{
    m_reloadAction->setEnabled(false);
    m_stopAction->setEnabled(true);
}

void BrowserWindow::onLoadProgress(int progress)
{
    m_progressBar->setValue(progress);
    if (progress >= 0 && progress < 100)
        m_progressBar->show();
    else
        m_progressBar->hide();
}

void BrowserWindow::onLoadFinished(bool ok)
{
    m_progressBar->hide();
    m_reloadAction->setEnabled(true);
    m_stopAction->setEnabled(false);

    QWebEngineView *view = currentView();
    if (!ok && view && view->url().isValid()) {
        m_securityLabel->setText(tr("Load failed"));
        return;
    }

    updateSecurityIndicator(view ? view->url() : QUrl());
    updateNavigationState();
}

void BrowserWindow::updateNavigationState()
{
    QWebEngineView *view = currentView();
    if (!view)
        return;

    const bool canGoBack = view->history()->canGoBack();
    const bool canGoForward = view->history()->canGoForward();
    m_backAction->setEnabled(canGoBack);
    m_forwardAction->setEnabled(canGoForward);
}

void BrowserWindow::updateWindowTitle()
{
    QWebEngineView *view = currentView();
    const QString pageTitle = view ? view->title().trimmed() : QString();
    if (pageTitle.isEmpty())
        setWindowTitle(QStringLiteral("SpacePenguin"));
    else
        setWindowTitle(pageTitle + QStringLiteral(" - SpacePenguin"));
}

void BrowserWindow::updateSecurityIndicator(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("https")) {
        m_securityLabel->setText(tr("Secure"));
        m_securityLabel->setToolTip(tr("Connection is encrypted"));
    } else if (scheme == QLatin1String("http")) {
        m_securityLabel->setText(tr("Not secure"));
        m_securityLabel->setToolTip(tr("Connection is not encrypted"));
    } else {
        m_securityLabel->setText(QString());
        m_securityLabel->setToolTip(QString());
    }
}

void BrowserWindow::setZoom(qreal factor)
{
    QWebEngineView *view = currentView();
    if (!view)
        return;

    view->setZoomFactor(qBound(kMinimumZoom, factor, kMaximumZoom));
}

qreal BrowserWindow::currentZoom() const
{
    const QWebEngineView *view = currentView();
    return view ? view->zoomFactor() : qreal(1.0);
}

void BrowserWindow::restoreSession()
{
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    const QByteArray state = settings.value(QStringLiteral("window/state")).toByteArray();
    if (!state.isEmpty())
        restoreState(state);

    const QStringList urls =
        settings.value(QStringLiteral("session/urls")).toStringList().mid(0, kMaximumRestoredTabs);

    if (urls.isEmpty()) {
        newTab();
        return;
    }

    for (const QString &url : urls)
        newTab(QUrl(url));

    const int index = settings.value(QStringLiteral("session/currentTab")).toInt();
    m_tabs->setCurrentIndex(qBound(0, index, m_tabs->count() - 1));
}

void BrowserWindow::saveSession() const
{
    QSettings settings;

    QStringList urls;
    urls.reserve(m_tabs->count());
    for (int index = 0; index < m_tabs->count(); ++index) {
        const QWebEngineView *view = qobject_cast<QWebEngineView *>(m_tabs->widget(index));
        if (view && view->url().isValid())
            urls.append(view->url().toString());
    }

    settings.setValue(QStringLiteral("session/urls"), urls);
    settings.setValue(QStringLiteral("session/currentTab"), m_tabs->currentIndex());
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    settings.setValue(QStringLiteral("browser/home"), m_resolver.homeUrlString());
    settings.setValue(QStringLiteral("browser/search"), m_resolver.searchTemplate());
}

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    saveSession();
    QMainWindow::closeEvent(event);
}

QWebEngineView *BrowserWindow::currentView() const
{
    return viewAt(m_tabs ? m_tabs->currentIndex() : -1);
}

QWebEngineView *BrowserWindow::viewAt(int index) const
{
    if (index < 0 || index >= m_tabs->count())
        return nullptr;
    return qobject_cast<QWebEngineView *>(m_tabs->widget(index));
}

}
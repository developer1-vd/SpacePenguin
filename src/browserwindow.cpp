#include "browserwindow.h"

#include "aboutpages.h"
#include "adblocker.h"
#include "browserpage.h"
#include "downloadmanager.h"
#include "extensionsdialog.h"
#include "historymanager.h"
#include "settingsdialog.h"
#include "userextensions.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QWebEngineFindTextResult>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

namespace spacepenguin {
namespace {

constexpr qreal kMinimumZoom = 0.5;
constexpr qreal kMaximumZoom = 2.0;
constexpr qreal kZoomStep = 0.1;
constexpr int kMaximumRestoredTabs = 32;
constexpr int kMaximumClosedTabs = 16;

QIcon themeIcon(const QString &name, QStyle::StandardPixmap fallback)
{
    const QIcon icon = QIcon::fromTheme(name);
    if (!icon.isNull())
        return icon;
    return QApplication::style()->standardIcon(fallback);
}

} // namespace

BrowserWindow::BrowserWindow(ProfileServices services, QWidget *parent)
    : QMainWindow(parent)
    , m_services(std::move(services))
    , m_isPrivate(m_services.profile && m_services.profile->isOffTheRecord())
{
    setWindowTitle(QStringLiteral("SpacePenguin"));
    resize(1100, 720);

    applyTheme(Theme::storedMode());

    createActions();
    createToolBar();
    createMenus();
    createTabWidget();
    createFindBar();
    createTabShortcuts();
    createStatusControls();

    statusBar()->addPermanentWidget(m_securityLabel);
    statusBar()->addPermanentWidget(m_progressBar, 1);

    restoreSession();
}

BrowserWindow::~BrowserWindow() = default;

void BrowserWindow::createActions()
{
    m_newTabAction = new QAction(themeIcon(QStringLiteral("tab-new"),
                                           QStyle::SP_FileDialogNewFolder), tr("New Tab"), this);
    m_newTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(m_newTabAction, &QAction::triggered, this, [this] { newTab(m_resolver.homeUrl()); });

    m_newWindowAction = new QAction(tr("New Window"), this);
    m_newWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(m_newWindowAction, &QAction::triggered, this, [this] { newWindow(m_isPrivate); });

    m_newPrivateWindowAction = new QAction(tr("New Private Window"), this);
    m_newPrivateWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(m_newPrivateWindowAction, &QAction::triggered, this, [this] { newWindow(true); });

    m_closeTabAction = new QAction(tr("Close Tab"), this);
    m_closeTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(m_closeTabAction, &QAction::triggered, this,
            [this] { closeTab(m_tabs->currentIndex()); });

    m_closeWindowAction = new QAction(tr("Close Window"), this);
    m_closeWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    connect(m_closeWindowAction, &QAction::triggered, this, &QWidget::close);

    m_reopenTabAction = new QAction(tr("Reopen Closed Tab"), this);
    m_reopenTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    m_reopenTabAction->setEnabled(false);
    connect(m_reopenTabAction, &QAction::triggered, this, &BrowserWindow::reopenClosedTab);

    m_nextTabAction = new QAction(tr("Next Tab"), this);
    m_nextTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab));
    connect(m_nextTabAction, &QAction::triggered, this, [this] { selectTabByOffset(1); });

    m_previousTabAction = new QAction(tr("Previous Tab"), this);
    m_previousTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    connect(m_previousTabAction, &QAction::triggered, this, [this] { selectTabByOffset(-1); });

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
    m_stopAction->setShortcut(QKeySequence(Qt::Key_Escape));
    m_stopAction->setEnabled(false);
    connect(m_stopAction, &QAction::triggered, this,
            [this] { if (QWebEngineView *view = currentView()) view->stop(); });

    m_homeAction = new QAction(themeIcon(QStringLiteral("go-home"),
                                         QStyle::SP_DirHomeIcon), tr("Home"), this);
    m_homeAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Home));
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

    m_findAction = new QAction(themeIcon(QStringLiteral("edit-find"),
                                         QStyle::SP_FileDialogContentsView), tr("Find in Page"), this);
    m_findAction->setShortcut(QKeySequence::Find);
    connect(m_findAction, &QAction::triggered, this, &BrowserWindow::showFindBar);

    m_shortcutsAction = new QAction(tr("Keyboard Shortcuts"), this);
    m_shortcutsAction->setShortcut(QKeySequence::HelpContents);
    connect(m_shortcutsAction, &QAction::triggered, this, [this] {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Keyboard Shortcuts"));
        dialog.resize(520, 460);

        auto *layout = new QVBoxLayout(&dialog);
        auto *tree = new QTreeWidget(&dialog);
        tree->setColumnCount(2);
        tree->setHeaderLabels({tr("Shortcut"), tr("Action")});
        tree->setRootIsDecorated(false);
        tree->setAlternatingRowColors(true);

        const auto actions = findChildren<QAction *>();
        for (QAction *action : actions) {
            if (!action->shortcut().isEmpty() && !action->text().isEmpty()
                && !action->text().startsWith(QLatin1String("Focus Address"))) {
                auto *item = new QTreeWidgetItem(tree);
                item->setText(0, action->shortcut().toString(QKeySequence::PortableText));
                item->setText(1, action->text());
            }
        }
        tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        layout->addWidget(tree);
        layout->addWidget(buttons);
        dialog.exec();
    });
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

    m_downloadsAction = new QAction(themeIcon(QStringLiteral("document-open"),
                                                QStyle::SP_DialogOpenButton),
                                    tr("Downloads"), this);
    m_downloadsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
    connect(m_downloadsAction, &QAction::triggered, this, &BrowserWindow::showDownloads);
    toolBar->addAction(m_downloadsAction);

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

void BrowserWindow::createFindBar()
{
    m_findToolBar = addToolBar(tr("Find"));
    m_findToolBar->setMovable(false);
    m_findToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_findToolBar->hide();

    m_findToolBar->addWidget(new QLabel(tr("Find:"), m_findToolBar));

    m_findInput = new QLineEdit;
    m_findInput->setPlaceholderText(tr("Find in page"));
    m_findInput->setClearButtonEnabled(true);
    m_findToolBar->addWidget(m_findInput);

    auto *previousButton = new QToolButton;
    previousButton->setText(tr("Previous"));
    previousButton->setToolTip(tr("Previous match"));
    connect(previousButton, &QToolButton::clicked, this, [this] { findNext(true); });
    m_findToolBar->addWidget(previousButton);

    auto *nextButton = new QToolButton;
    nextButton->setText(tr("Next"));
    nextButton->setToolTip(tr("Next match"));
    connect(nextButton, &QToolButton::clicked, this, [this] { findNext(false); });
    m_findToolBar->addWidget(nextButton);

    m_findStatus = new QLabel;
    m_findToolBar->addWidget(m_findStatus);

    auto *closeButton = new QToolButton;
    closeButton->setText(tr("Close"));
    closeButton->setToolTip(tr("Close the find bar"));
    connect(closeButton, &QToolButton::clicked, this, [this] {
        m_findInput->clear();
        m_findToolBar->hide();
        m_findInput->setFocus();
    });
    m_findToolBar->addWidget(closeButton);

    connect(m_findInput, &QLineEdit::returnPressed, this, [this] { findNext(false); });
    connect(m_findInput, &QLineEdit::textChanged, this, [this] { findNext(false); });
}

void BrowserWindow::createTabShortcuts()
{
    for (int number = 1; number <= 9; ++number) {
        auto *shortcut = new QShortcut(QKeySequence(Qt::CTRL | (Qt::Key_1 + (number - 1))), this);
        connect(shortcut, &QShortcut::activated, this, [this, number] { selectTabByIndex(number - 1); });
    }

    auto *historyShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_H), this);
    connect(historyShortcut, &QShortcut::activated, this, [this] {
        loadAboutPage(QStringLiteral("about:history"));
    });
}

void BrowserWindow::createStatusControls()
{
    m_blockingButton = new QToolButton;
    m_blockingButton->setObjectName(QStringLiteral("blockingButton"));
    m_blockingButton->setAutoRaise(true);
    m_blockingButton->setAccessibleName(tr("Ad blocking"));
    m_blockingButton->setToolTip(tr("Ad blocking"));
    m_blockingButton->setPopupMode(QToolButton::InstantPopup);

    m_blockingMenu = new QMenu(this);
    m_blockingButton->setMenu(m_blockingMenu);
    connect(m_blockingMenu, &QMenu::aboutToShow, this, &BrowserWindow::populateBlockingMenu);
    m_blockingButton->setIcon(themeIcon(QStringLiteral("edit-delete"),
                                        QStyle::SP_DialogApplyButton));

    if (m_services.adBlocker) {
        connect(m_services.adBlocker, &AdBlocker::blockedCountChanged, this,
                &BrowserWindow::updateBlockingIndicator);
        connect(m_services.adBlocker, &AdBlocker::enabledChanged, this,
                [this] { updateBlockingIndicator(); });
        updateBlockingIndicator();
    }

    statusBar()->addPermanentWidget(m_blockingButton);
}

void BrowserWindow::updateBlockingIndicator()
{
    if (!m_blockingButton || !m_services.adBlocker)
        return;

    AdBlocker *blocker = m_services.adBlocker;
    if (blocker->ruleCount() == 0) {
        m_blockingButton->setText(tr("No filters"));
        m_blockingButton->setToolTip(
            tr("No filter list is loaded. Put rules in %1").arg(blocker->listPath()));
        return;
    }

    const QString count = tr("Blocked %1").arg(blocker->blockedCount());
    m_blockingButton->setText(blocker->isEnabled() ? count : tr("Blocking off"));
    m_blockingButton->setToolTip(
        blocker->isEnabled()
            ? tr("%1 requests blocked with %2 rules. Click for options.")
                  .arg(blocker->blockedCount())
                  .arg(blocker->ruleCount())
            : tr("Ad blocking is paused. %1 rules are loaded.").arg(blocker->ruleCount()));
}

void BrowserWindow::populateBlockingMenu()
{
    if (!m_services.adBlocker || !m_blockingMenu)
        return;

    m_blockingMenu->clear();
    AdBlocker *blocker = m_services.adBlocker;

    if (QWebEngineView *view = currentView()) {
        const QString host = view->url().host();
        if (!host.isEmpty()) {
            const bool paused = blocker->isPausedFor(host);
            QAction *pauseAction = m_blockingMenu->addAction(
                paused ? tr("Resume blocking on %1").arg(host)
                       : tr("Pause blocking on %1").arg(host));
            connect(pauseAction, &QAction::triggered, this, [this, host, paused] {
                QStringList hosts = m_services.adBlocker->pausedHosts();
                hosts.removeAll(host);
                if (!paused)
                    hosts.append(host);
                m_services.adBlocker->setPausedHosts(hosts);
            });
            m_blockingMenu->addSeparator();
        }
    }

    QAction *toggleAction =
        m_blockingMenu->addAction(blocker->isEnabled() ? tr("Disable ad blocking")
                                                      : tr("Enable ad blocking"));
    connect(toggleAction, &QAction::triggered, this,
            [this] { m_services.adBlocker->setEnabled(!m_services.adBlocker->isEnabled()); });

    if (!blocker->lastListError().isEmpty()) {
        m_blockingMenu->addSeparator();
        m_blockingMenu->addAction(blocker->lastListError())->setEnabled(false);
    }
}

void BrowserWindow::applyTheme(Theme::Mode mode)
{
    Theme::storeMode(mode);
    Theme::apply(mode);
}

void BrowserWindow::showExtensionsDialog()
{
    if (!m_services.userScripts)
        return;

    ExtensionsDialog dialog(m_services.userScripts, this);
    dialog.exec();
}

void BrowserWindow::showSettings()
{
    if (!m_services.adBlocker || !m_services.bookmarkManager ||
        !m_services.cookieManager || !m_services.dataSaverManager ||
        !m_services.downloadManager || !m_services.historyManager ||
        !m_services.userScripts) {
        return;
    }

    SettingsDialog dialog(
        m_services.adBlocker,
        m_services.bookmarkManager,
        m_services.cookieManager,
        m_services.dataSaverManager,
        m_services.downloadManager,
        m_services.historyManager,
        m_services.userScripts,
        this);
    dialog.exec();
}

void BrowserWindow::showDownloads()
{
    if (!m_services.downloadManager)
        return;

    const auto downloads = m_services.downloadManager->downloads();
    if (downloads.isEmpty()) {
        // Navigate to an about:downloads page or show a dialog
        loadAboutPage(QStringLiteral("about:downloads"));
        return;
    }

    // For now, just show the first download in a message box
    // TODO: Create a proper downloads dialog
    const DownloadItem &item = downloads.first();
    QMessageBox::information(this, tr("Downloads"),
                             tr("Download: %1\n%2 of %3 bytes")
                             .arg(item.suggestedFileName)
                             .arg(item.receivedBytes)
                             .arg(item.totalBytes));
}

void BrowserWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_newTabAction);
    fileMenu->addAction(m_newWindowAction);
    fileMenu->addAction(m_newPrivateWindowAction);
    fileMenu->addAction(m_reopenTabAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_closeTabAction);
    fileMenu->addAction(m_closeWindowAction);
    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction(tr("Quit"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_reloadAction);
    viewMenu->addAction(m_stopAction);
    viewMenu->addAction(m_homeAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_findAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_zoomResetAction);

    QMenu *themeMenu = viewMenu->addMenu(tr("Theme"));
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    const Theme::Mode currentMode = Theme::storedMode();
    for (const Theme::Mode mode : Theme::modes()) {
        QAction *action = themeMenu->addAction(Theme::modeLabel(mode));
        action->setCheckable(true);
        action->setChecked(mode == currentMode);
        action->setData(static_cast<int>(mode));
        connect(action, &QAction::triggered, this,
                [this, mode] { applyTheme(mode); });
        m_themeGroup->addAction(action);
    }

    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction *extensionsAction = toolsMenu->addAction(tr("Extensions…"));
    extensionsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    connect(extensionsAction, &QAction::triggered, this, &BrowserWindow::showExtensionsDialog);
    toolsMenu->addSeparator();
    QAction *blockingAction = toolsMenu->addAction(tr("Ad blocking…"));
    connect(blockingAction, &QAction::triggered, this, [this] {
        if (!m_blockingMenu)
            return;
        m_blockingMenu->popup(m_blockingButton
                                  ? m_blockingButton->mapToGlobal(QPoint(0, m_blockingButton->height()))
                                  : mapToGlobal(QPoint(0, height())));
    });

    toolsMenu->addSeparator();
    QAction *settingsAction = toolsMenu->addAction(tr("Settings…"));
    settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(settingsAction, &QAction::triggered, this, &BrowserWindow::showSettings);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_shortcutsAction);

    QAction *aboutAction = helpMenu->addAction(tr("About SpacePenguin"));
    aboutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
    aboutAction->setMenuRole(QAction::AboutRole);
    connect(aboutAction, &QAction::triggered, this, [this] {
        if (currentView())
            loadAboutPage(QStringLiteral("about:about"));
    });

    QAction *aboutQtAction = helpMenu->addAction(tr("About Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    connect(aboutQtAction, &QAction::triggered, this, [] { QApplication::aboutQt(); });
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
    view->setPage(new BrowserPage(m_services.profile, view));
    view->setZoomFactor(1.0);
    connectView(view);

    m_tabs->addTab(view, tr("New Tab"));
    m_tabs->setCurrentWidget(view);

    const QUrl target = url.isEmpty() ? m_resolver.homeUrl() : url;
    if (target.scheme() == QLatin1String("about"))
        loadAboutPage(target.toString());
    else
        view->setUrl(target);
}

void BrowserWindow::newWindow(bool isPrivate)
{
    ProfileServices services;
    if (isPrivate) {
        services = createProfileServices(this, true);
    } else {
        services = createProfileServices(this, false);
    }

    auto *window = new BrowserWindow(std::move(services));
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
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
    if (const auto *view = qobject_cast<QWebEngineView *>(page)) {
        if (view->url().isValid() && view->url().scheme() != QLatin1String("about")) {
            m_closedTabs.prepend(ClosedTab{index, view->url()});
            while (m_closedTabs.size() > kMaximumClosedTabs)
                m_closedTabs.removeLast();
            m_reopenTabAction->setEnabled(true);
        }
    }

    m_tabs->removeTab(index);
    page->deleteLater();

    if (m_tabs->count() == 0)
        close();
}

void BrowserWindow::onTabCloseRequested(int index)
{
    closeTab(index);
}

void BrowserWindow::reopenClosedTab()
{
    if (m_closedTabs.isEmpty())
        return;

    const ClosedTab closed = m_closedTabs.takeFirst();
    m_reopenTabAction->setEnabled(!m_closedTabs.isEmpty());

    newTab(closed.url);
    const int index = qBound(0, closed.index, m_tabs->count() - 1);
    m_tabs->setCurrentIndex(index);
}

void BrowserWindow::selectTabByOffset(int offset)
{
    if (m_tabs->count() < 2)
        return;
    const int index = (m_tabs->currentIndex() + offset + m_tabs->count()) % m_tabs->count();
    m_tabs->setCurrentIndex(index);
}

void BrowserWindow::selectTabByIndex(int index)
{
    if (index < m_tabs->count())
        m_tabs->setCurrentIndex(index);
}

void BrowserWindow::goHome()
{
    if (QWebEngineView *view = currentView())
        view->setUrl(m_resolver.homeUrl());
}

void BrowserWindow::navigate(const QString &text)
{
    QWebEngineView *view = currentView();
    if (!view)
        return;

    const QUrl url = m_resolver.resolve(text);
    if (url.scheme() == QLatin1String("about"))
        loadAboutPage(url.toString());
    else
        view->setUrl(url);
}

AboutPageContext BrowserWindow::aboutContext() const
{
    AboutPageContext context;
    context.appVersion = QCoreApplication::applicationVersion();
    context.isPrivate = m_isPrivate;
    context.platform = QGuiApplication::platformName();
    context.theme = Theme::modeLabel(Theme::storedMode());
    context.buildType = QStringLiteral(SPACEPENGUIN_BUILD_TYPE);

    if (m_services.adBlocker) {
        context.filterRules = m_services.adBlocker->ruleCount();
        context.cosmeticRules = m_services.adBlocker->cosmeticRuleCount();
        context.blockingEnabled = m_services.adBlocker->isEnabled();
        context.blockedRequests = m_services.adBlocker->blockedCount();
    }

    if (m_services.userScripts)
        context.extensionCount = m_services.userScripts->scripts().size();

    return context;
}

void BrowserWindow::loadAboutPage(const QString &id)
{
    QWebEngineView *view = currentView();
    if (!view)
        return;

    const AboutPageContext context = aboutContext();
    const QString html = AboutPages::isKnown(id) ? AboutPages::render(id, context)
                                                 : AboutPages::renderUnknown(id);

    view->page()->setContent(html.toUtf8(), QStringLiteral("text/html"), QUrl(id));
}

void BrowserWindow::onOmniboxReturnPressed()
{
    navigate(m_omnibox->text());
    m_omniboxEdited = false;
    m_omnibox->setFocus();
}

void BrowserWindow::showFindBar()
{
    m_findToolBar->show();
    m_findInput->setFocus();
    m_findInput->selectAll();
}

void BrowserWindow::findNext(bool backwards)
{
    QWebEngineView *view = currentView();
    if (!view || m_findInput->text().isEmpty()) {
        m_findStatus->clear();
        return;
    }

    QWebEnginePage::FindFlags flags;
    if (backwards)
        flags |= QWebEnginePage::FindBackward;

    view->page()->findText(m_findInput->text(), flags,
                           [this](const QWebEngineFindTextResult &result) {
                               findMatchesShown(result.numberOfMatches(), result.activeMatch());
                           });
}

void BrowserWindow::findMatchesShown(int matchCount, int activeMatch)
{
    if (matchCount <= 0) {
        m_findStatus->setText(tr("No results"));
        return;
    }

    m_findStatus->setText(tr("%1 of %2").arg(activeMatch).arg(matchCount));
}

void BrowserWindow::syncOmnibox(const QUrl &url)
{
    if (!m_omnibox->hasFocus() || !m_omniboxEdited)
        m_omnibox->setText(url.toDisplayString());
    m_omniboxEdited = false;
}

bool BrowserWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_omnibox && event->type() == QEvent::FocusOut) {
        m_omniboxEdited = false;
        if (m_omnibox->text().isEmpty())
            m_omnibox->setText(currentView() ? currentView()->url().toDisplayString() : QString());
    }

    if (watched == m_findInput && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            m_findToolBar->hide();
            return true;
        }
    }

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

    const QString label = url.scheme() == QLatin1String("about")
        ? AboutPages::pageTitle(url.toString())
        : (url.host().isEmpty() ? tr("New Tab") : url.host());
    m_tabs->setTabText(m_tabs->currentIndex(), label);
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
    if (!ok && view && view->url().isValid() && view->url().scheme() != QLatin1String("about")) {
        m_securityLabel->setText(tr("Load failed"));
        return;
    }

    if (ok && view && m_services.historyManager) {
        const QUrl url = view->url();
        const QString title = view->title();
        m_services.historyManager->addVisit(url, title);
    }

    updateSecurityIndicator(view ? view->url() : QUrl());
    updateNavigationState();
}

void BrowserWindow::updateNavigationState()
{
    QWebEngineView *view = currentView();
    if (!view)
        return;

    m_backAction->setEnabled(view->history()->canGoBack());
    m_forwardAction->setEnabled(view->history()->canGoForward());
}

void BrowserWindow::updateWindowTitle()
{
    QWebEngineView *view = currentView();
    const QString pageTitle = view ? view->title().trimmed() : QString();

    QString title;
    if (pageTitle.isEmpty())
        title = QStringLiteral("SpacePenguin");
    else
        title = pageTitle + QStringLiteral(" - SpacePenguin");

    if (m_isPrivate)
        title += QStringLiteral(" (Private)");

    setWindowTitle(title);
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
    } else if (scheme == QLatin1String("sp")) {
        m_securityLabel->setText(tr("Bundled page"));
        m_securityLabel->setToolTip(tr("Served by SpacePenguin itself"));
    } else if (scheme == QLatin1String("about")) {
        m_securityLabel->setText(tr("Internal page"));
        m_securityLabel->setToolTip(tr("Rendered by SpacePenguin"));
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
    if (m_isPrivate) {
        newTab();
        return;
    }

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
    if (m_isPrivate)
        return;

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
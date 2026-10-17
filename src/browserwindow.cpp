#include "browserwindow.h"

#include "aboutpages.h"
#include "adblocker.h"
#include "bookmarkmanager.h"
#include "bookmarksdialog.h"
#include "chromeextensions.h"
#include "browserpage.h"
#include "cookiemanager.h"
#include "datasavermanager.h"
#include "downloadmanager.h"
#include "downloadsdialog.h"
#include "extensionsdialog.h"
#include "historydialog.h"
#include "historymanager.h"
#include "logins.h"
#include "notifications.h"
#include "icons.h"
#include "preferences.h"
#include "devicetuning.h"
#include "sandbox.h"
#include "settingsdialog.h"
#include "shortcuts.h"
#include "sidetabs.h"
#include "sitepermissions.h"
#include "startuptrace.h"
#include "tabgroups.h"
#include "userextensions.h"
#include "webview.h"
#include "windowframe.h"
#include "youtube.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCompleter>
#include <QCursor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QRegularExpression>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWebEngineFindTextResult>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QWebEnginePermission>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWidgetAction>

namespace spacepenguin {
namespace {

constexpr qreal kMinimumZoom = 0.5;
constexpr qreal kMaximumZoom = 2.0;
constexpr qreal kZoomStep = 0.1;
constexpr int kMaximumRestoredTabs = 32;
constexpr int kMaximumClosedTabs = 16;
constexpr int kMaximumCompletions = 400;
constexpr int kLinkPreviewMargin = 6;

/**
 * Profiles shared between windows. Two persistent profiles with the same storage
 * name in one process corrupt each other, so every normal window uses one set of
 * services, and every private window shares one off-the-record set that is thrown
 * away when the last private window closes.
 */
struct ServiceRegistry {
    ProfileServices normal;
    ProfileServices privateServices;
    int privateWindows = 0;
    bool sessionRestored = false;
    bool quitting = false;
};

ServiceRegistry &registry()
{
    static ServiceRegistry instance;
    return instance;
}

void releaseServices(ProfileServices &services)
{
    // A private set may borrow the normal window's bookmarks; those stay.
    if (services.bookmarkManager == registry().normal.bookmarkManager)
        services.bookmarkManager = nullptr;

    // The profile goes first: its interceptor and script collection belong to
    // services that are deleted in the same batch.
    const QList<QObject *> objects = {
        services.profile, services.userScripts, services.downloadManager,
        services.cookieManager, services.historyManager, services.bookmarkManager,
        services.dataSaverManager, services.adBlocker,
    };
    for (QObject *object : objects) {
        if (object)
            object->deleteLater();
    }
    services = ProfileServices{};
}

bool isWebScheme(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    return scheme == QLatin1String("http") || scheme == QLatin1String("https");
}

bool isStartPage(const QUrl &url)
{
    return url.scheme() == QLatin1String("sp");
}

QToolButton *chromeButton(QAction *action, QWidget *parent = nullptr)
{
    auto *button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setIconSize(QSize(18, 18));
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QString formatPercent(qreal factor)
{
    return QStringLiteral("%1%").arg(qRound(factor * 100));
}

} // namespace

// ---- BrowserWindow ----------------------------------------------------------

BrowserWindow::BrowserWindow(ProfileServices services, QWidget *parent)
    : QMainWindow(parent)
    , m_services(std::move(services))
    , m_isPrivate(m_services.profile && m_services.profile->isOffTheRecord())
{
    ServiceRegistry &shared = registry();
    if (m_isPrivate) {
        if (!shared.privateServices.profile)
            shared.privateServices = m_services;
        ++shared.privateWindows;
    } else if (!shared.normal.profile) {
        shared.normal = m_services;
    }

    setWindowTitle(QStringLiteral("SpacePenguin"));
    setObjectName(QStringLiteral("browserWindow"));
    resize(1200, 780);
    WindowFrame::prepareWindow(this);
    m_frame = new WindowFrame(this);

    Theme::apply(Theme::storedMode());

    m_groups = new TabGroups(this);
    connect(m_groups, &TabGroups::changed, this, &BrowserWindow::refreshGroups);

    createActions();
    createChrome();
    createMenus();
    createTabShortcuts();

    Preferences *preferences = Preferences::instance();
    connect(preferences, &Preferences::changed, this, &BrowserWindow::applyPreferences);
    applyPreferences();

    if (m_services.adBlocker) {
        connect(m_services.adBlocker, &AdBlocker::blockedCountChanged, this,
                &BrowserWindow::updateBlockingIndicator);
        connect(m_services.adBlocker, &AdBlocker::enabledChanged, this,
                &BrowserWindow::updateBlockingIndicator);
        connect(m_services.adBlocker, &AdBlocker::filtersChanged, this,
                &BrowserWindow::updateBlockingIndicator);
    }

    if (m_services.bookmarkManager) {
        const auto bookmarksChanged = [this] {
            rebuildBookmarksBar();
            updateBookmarkStar();
            m_completionsDirty = true;
        };
        connect(m_services.bookmarkManager, &BookmarkManager::bookmarkAdded, this, bookmarksChanged);
        connect(m_services.bookmarkManager, &BookmarkManager::bookmarkRemoved, this, bookmarksChanged);
        connect(m_services.bookmarkManager, &BookmarkManager::bookmarkUpdated, this, bookmarksChanged);
    }

    if (m_services.historyManager) {
        const auto historyChanged = [this] { m_completionsDirty = true; };
        connect(m_services.historyManager, &HistoryManager::visitAdded, this, historyChanged);
        connect(m_services.historyManager, &HistoryManager::visitRemoved, this, historyChanged);
        connect(m_services.historyManager, &HistoryManager::historyCleared, this, historyChanged);
    }

    if (m_services.logins)
        connect(m_services.logins, &LoginStore::changed, this, &BrowserWindow::updateLoginsAction);

    if (m_services.extensions) {
        connect(m_services.extensions, &ChromeExtensions::installed, this, [this](const QString &name) {
            if (isActiveWindow())
                showToast(tr("Added “%1”. Find it under the puzzle piece in the toolbar.").arg(name), 5000);
        });
        connect(m_services.extensions, &ChromeExtensions::failed, this, [this](const QString &message) {
            if (isActiveWindow())
                showToast(message, 8000);
        });
    }

    // A clicked notification brings its site's tab forward.
    connect(Notifications::instance(), &Notifications::clicked, this, [this](const QUrl &origin) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            const WebView *view = viewAt(i);
            if (view && view->url().host() == origin.host() && view->url().scheme() == origin.scheme()) {
                m_tabBar->setCurrentIndex(i);
                showNormal();
                raise();
                activateWindow();
                return;
            }
        }
    });
    connect(Notifications::instance(), &Notifications::shownInWindow, this,
            [this](const QString &title, const QString &message) {
                if (isActiveWindow())
                    showToast(message.isEmpty() ? title : QStringLiteral("%1 — %2").arg(title, message), 6000);
            });

    if (m_services.downloadManager) {
        connect(m_services.downloadManager, &DownloadManager::downloadStarted, this,
                [this](const DownloadItem &item) {
                    m_downloadsButton->setToolTip(
                        tr("Downloading %1 — click to see downloads").arg(item.suggestedFileName));
                    showToast(tr("Downloading “%1”…").arg(item.suggestedFileName), 3000);
                });
    }

    if (m_services.dataSaverManager) {
        connect(m_services.dataSaverManager, &DataSaverManager::modeChanged, this, [this] {
            for (int index = 0; index < m_tabBar->count(); ++index)
                applyDataSaver(viewAt(index));
        });
    }

    rebuildBookmarksBar();
    restoreSession();
}

BrowserWindow::~BrowserWindow()
{
    ServiceRegistry &shared = registry();
    if (m_tabBar)
        m_tabBar->disconnect(this);
    if (m_isPrivate) {
        if (--shared.privateWindows <= 0 && shared.privateServices.profile == m_services.profile) {
            // Close the tabs first so no page outlives its profile.
            while (m_tabBar && m_tabBar->count() > 0) {
                WebView *view = viewAt(0);
                m_tabBar->removeTab(0);
                delete view;
            }
            shared.privateWindows = 0;
            releaseServices(shared.privateServices);
        }
    }
}

// ---- Construction -------------------------------------------------------------

void BrowserWindow::createActions()
{
    const auto makeAction = [this](const QString &text, const QList<QKeySequence> &shortcuts = {},
                                   const QIcon &icon = QIcon()) {
        auto *action = new QAction(icon, text, this);
        if (!shortcuts.isEmpty())
            action->setShortcuts(shortcuts);
        // Actions live on the window so their shortcuts work with the menu bar hidden.
        addAction(action);
        return action;
    };

    m_newTabAction = makeAction(tr("New Tab"), Shortcuts::keys(Shortcuts::NewTab),
                                Icons::icon(Icons::Plus));
    connect(m_newTabAction, &QAction::triggered, this, [this] {
        newTab();
        m_omnibox->setFocus();
    });

    m_newWindowAction = makeAction(tr("New Window"), Shortcuts::keys(Shortcuts::NewWindow));
    connect(m_newWindowAction, &QAction::triggered, this, [this] { newWindow(m_isPrivate); });

    m_newPrivateWindowAction = makeAction(tr("New Private Window"),
                                          Shortcuts::keys(Shortcuts::NewPrivateWindow),
                                          Icons::icon(Icons::Private));
    connect(m_newPrivateWindowAction, &QAction::triggered, this, [this] { newWindow(true); });

    m_closeTabAction = makeAction(tr("Close Tab"), Shortcuts::keys(Shortcuts::CloseTab));
    connect(m_closeTabAction, &QAction::triggered, this,
            [this] { closeTab(m_tabBar->currentIndex()); });

    m_closeWindowAction = makeAction(tr("Close Window"),
                                     Shortcuts::keys(Shortcuts::CloseWindow));
    connect(m_closeWindowAction, &QAction::triggered, this, &QWidget::close);

    m_reopenTabAction = makeAction(tr("Reopen Closed Tab"),
                                   Shortcuts::keys(Shortcuts::ReopenClosedTab));
    m_reopenTabAction->setEnabled(false);
    connect(m_reopenTabAction, &QAction::triggered, this, &BrowserWindow::reopenClosedTab);

    m_nextTabAction = makeAction(tr("Next Tab"), Shortcuts::keys(Shortcuts::NextTab));
    connect(m_nextTabAction, &QAction::triggered, this, [this] { selectTabByOffset(1); });

    m_previousTabAction = makeAction(tr("Previous Tab"),
                                     Shortcuts::keys(Shortcuts::PreviousTab));
    connect(m_previousTabAction, &QAction::triggered, this, [this] { selectTabByOffset(-1); });

    m_quitAction = makeAction(tr("Quit"), Shortcuts::keys(Shortcuts::Quit));
    m_quitAction->setMenuRole(QAction::QuitRole);
    connect(m_quitAction, &QAction::triggered, this, [this] {
        // Keep the session of the window the user quit from, then close everything.
        saveSession();
        registry().quitting = true;
        QApplication::closeAllWindows();
    });

    m_backAction = makeAction(tr("Back"), Shortcuts::keys(Shortcuts::Back), Icons::icon(Icons::Back));
    connect(m_backAction, &QAction::triggered, this,
            [this] { if (WebView *view = currentView()) view->back(); });

    m_forwardAction = makeAction(tr("Forward"), Shortcuts::keys(Shortcuts::Forward), Icons::icon(Icons::Forward));
    connect(m_forwardAction, &QAction::triggered, this,
            [this] { if (WebView *view = currentView()) view->forward(); });

    m_reloadAction = makeAction(tr("Reload"), Shortcuts::keys(Shortcuts::Reload), Icons::icon(Icons::Reload));
    connect(m_reloadAction, &QAction::triggered, this,
            [this] { if (WebView *view = currentView()) view->reload(); });

    m_stopAction = makeAction(tr("Stop"), Shortcuts::keys(Shortcuts::Stop), Icons::icon(Icons::Stop));
    m_stopAction->setEnabled(false);
    connect(m_stopAction, &QAction::triggered, this,
            [this] { if (WebView *view = currentView()) view->stop(); });

    m_homeAction = makeAction(tr("Home"), Shortcuts::keys(Shortcuts::Home),
                              Icons::icon(Icons::Home));
    connect(m_homeAction, &QAction::triggered, this, &BrowserWindow::goHome);

    m_focusAddressAction = makeAction(tr("Focus Address Bar"), Shortcuts::keys(Shortcuts::FocusAddressBar));
    connect(m_focusAddressAction, &QAction::triggered, this, [this] {
        m_omnibox->setFocus(Qt::ShortcutFocusReason);
        m_omnibox->selectAll();
    });

    m_findAction = makeAction(tr("Find in Page…"), Shortcuts::keys(Shortcuts::Find), Icons::icon(Icons::Search));
    connect(m_findAction, &QAction::triggered, this, &BrowserWindow::showFindBar);

    m_zoomInAction = makeAction(tr("Zoom In"), Shortcuts::keys(Shortcuts::ZoomIn), Icons::icon(Icons::Plus));
    connect(m_zoomInAction, &QAction::triggered, this,
            [this] { setZoom(currentZoom() + kZoomStep); });

    m_zoomOutAction = makeAction(tr("Zoom Out"), Shortcuts::keys(Shortcuts::ZoomOut));
    connect(m_zoomOutAction, &QAction::triggered, this,
            [this] { setZoom(currentZoom() - kZoomStep); });

    m_zoomResetAction = makeAction(tr("Reset Zoom"), Shortcuts::keys(Shortcuts::ZoomReset));
    connect(m_zoomResetAction, &QAction::triggered, this, [this] { setZoom(1.0); });

    m_fullScreenAction = makeAction(tr("Full Screen"), Shortcuts::keys(Shortcuts::FullScreen));
    m_fullScreenAction->setCheckable(true);
    connect(m_fullScreenAction, &QAction::triggered, this, &BrowserWindow::toggleFullScreen);

    m_bookmarkPageAction = makeAction(tr("Bookmark This Page…"), Shortcuts::keys(Shortcuts::BookmarkPage),
                                      Icons::icon(Icons::Star));
    connect(m_bookmarkPageAction, &QAction::triggered, this,
            &BrowserWindow::toggleBookmarkForCurrentPage);

    m_bookmarksBarAction = makeAction(tr("Show Bookmarks Bar"),
                                      Shortcuts::keys(Shortcuts::BookmarksBar));
    m_bookmarksBarAction->setCheckable(true);
    connect(m_bookmarksBarAction, &QAction::triggered, this,
            [](bool checked) { Preferences::instance()->setShowBookmarksBar(checked); });

    m_sideTabsAction = makeAction(tr("Tabs on the Side"), Shortcuts::keys(Shortcuts::SideTabs),
                                  Icons::icon(Icons::SideTabs));
    m_sideTabsAction->setCheckable(true);
    m_sideTabsAction->setToolTip(tr("Show tabs on the side (%1)").arg(Shortcuts::text(Shortcuts::SideTabs)));
    connect(m_sideTabsAction, &QAction::triggered, this,
            [](bool checked) { Preferences::instance()->setSideTabs(checked); });

    m_menuBarAction = makeAction(tr("Show Menu Bar"));
    m_menuBarAction->setCheckable(true);
    connect(m_menuBarAction, &QAction::triggered, this,
            [](bool checked) { Preferences::instance()->setShowMenuBar(checked); });

    m_bookmarksAction = makeAction(tr("Bookmarks…"), Shortcuts::keys(Shortcuts::Bookmarks),
                                   Icons::icon(Icons::Bookmarks));
    connect(m_bookmarksAction, &QAction::triggered, this, &BrowserWindow::showBookmarks);

    m_historyAction = makeAction(tr("History…"), Shortcuts::keys(Shortcuts::History),
                                 Icons::icon(Icons::History));
    connect(m_historyAction, &QAction::triggered, this, &BrowserWindow::showHistory);

    m_downloadsAction = makeAction(tr("Downloads…"), Shortcuts::keys(Shortcuts::Downloads),
                                   Icons::icon(Icons::Download));
    connect(m_downloadsAction, &QAction::triggered, this, &BrowserWindow::showDownloads);

    m_extensionsAction = makeAction(tr("Extensions…"), Shortcuts::keys(Shortcuts::Extensions),
                                    Icons::icon(Icons::Extensions));
    connect(m_extensionsAction, &QAction::triggered, this, &BrowserWindow::showExtensionsDialog);

    m_settingsAction = makeAction(tr("Settings…"), Shortcuts::keys(Shortcuts::Settings),
                                  Icons::icon(Icons::Settings));
    m_settingsAction->setMenuRole(QAction::PreferencesRole);
    connect(m_settingsAction, &QAction::triggered, this, &BrowserWindow::showSettings);

    m_shortcutsAction = makeAction(tr("Keyboard Shortcuts"), Shortcuts::keys(Shortcuts::KeyboardShortcuts));
    connect(m_shortcutsAction, &QAction::triggered, this, &BrowserWindow::showShortcuts);

    m_aboutAction = makeAction(tr("About SpacePenguin"), {},
                               Icons::icon(Icons::Info));
    m_aboutAction->setMenuRole(QAction::AboutRole);
    connect(m_aboutAction, &QAction::triggered, this, [this] {
        newTab(QUrl(QStringLiteral("about:about")));
    });
}

void BrowserWindow::createChrome()
{
    auto *central = new QWidget;
    central->setObjectName(QStringLiteral("chrome"));
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(createTabStrip());
    layout->addWidget(createNavigationBar());
    createBookmarksBar();
    layout->addWidget(m_bookmarksBar);

    // Said plainly in every window: pages are running without Chromium's sandbox.
    if (!Sandbox::isEnabled()) {
        auto *banner = new QWidget;
        banner->setObjectName(QStringLiteral("sandboxBanner"));
        banner->setAttribute(Qt::WA_StyledBackground);
        auto *bannerLayout = new QHBoxLayout(banner);
        bannerLayout->setContentsMargins(12, 6, 6, 6);
        auto *icon = new QLabel;
        icon->setPixmap(Icons::icon(Icons::Warning).pixmap(18, 18));
        bannerLayout->addWidget(icon);
        QString text = tr("<b>Web pages are not sandboxed:</b> %1.").arg(Sandbox::disabledReason().toHtmlEscaped());
        if (!Sandbox::fix().isEmpty())
            text += QLatin1Char(' ') + Sandbox::fix().toHtmlEscaped();
        auto *message = new QLabel(text);
        message->setWordWrap(true);
        message->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bannerLayout->addWidget(message, 1);
        auto *dismiss = new QToolButton;
        dismiss->setIcon(Icons::icon(Icons::Close));
        dismiss->setAutoRaise(true);
        dismiss->setToolTip(tr("Hide until next start"));
        bannerLayout->addWidget(dismiss, 0, Qt::AlignTop);
        // A rounded card, so it keeps clear of the window's edges.
        auto *bannerHolder = new QWidget;
        auto *holderLayout = new QVBoxLayout(bannerHolder);
        holderLayout->setContentsMargins(8, 6, 8, 2);
        holderLayout->addWidget(banner);
        connect(dismiss, &QToolButton::clicked, bannerHolder, &QWidget::hide);
        layout->addWidget(bannerHolder);
    }

    layout->addWidget(createPromptBar());

    m_progressBar = new QProgressBar;
    m_progressBar->setObjectName(QStringLiteral("loadProgress"));
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedHeight(2);
    m_progressBar->setVisible(false);

    // The progress line overlays the top of the page so it does not shift content.
    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    m_stack = new QStackedWidget;
    contentLayout->addWidget(m_stack);
    m_progressBar->setParent(content);
    content->installEventFilter(this);

    m_linkPreview = new QLabel(content);
    m_linkPreview->setObjectName(QStringLiteral("linkPreview"));
    m_linkPreview->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_linkPreview->hide();

    m_sideTabs = new SideTabs(m_tabBar, m_groups, m_newTabAction, m_isPrivate);
    m_sideTabs->hide();
    connect(m_sideTabs, &SideTabs::closeRequested, this, &BrowserWindow::closeTab);
    connect(m_sideTabs, &SideTabs::contextMenuRequested, this, &BrowserWindow::showTabMenu);
    connect(m_sideTabs, &SideTabs::groupToggleRequested, this, &BrowserWindow::toggleGroupCollapsed);
    connect(m_sideTabs, &SideTabs::groupMenuRequested, this, &BrowserWindow::showGroupMenu);
    connect(m_sideTabs, &SideTabs::collapseRequested, this,
            [](bool collapsed) { Preferences::instance()->setSideTabsCollapsed(collapsed); });
    connect(m_sideTabs, &SideTabs::topTabsRequested, this,
            [] { Preferences::instance()->setSideTabs(false); });

    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(m_sideTabs);
    body->addWidget(content, 1);
    layout->addLayout(body, 1);
    layout->addWidget(createFindBar());

    setCentralWidget(central);
}

QWidget *BrowserWindow::createTabStrip()
{
    m_tabStrip = new QWidget;
    m_tabStrip->setObjectName(QStringLiteral("tabStrip"));
    m_tabStrip->setAttribute(Qt::WA_StyledBackground);
    m_tabStrip->setProperty("private", m_isPrivate);

    auto *layout = new QHBoxLayout(m_tabStrip);
    layout->setContentsMargins(6, 0, 6, 0);
    layout->setSpacing(2);

    // Edge's place for it: the first thing in the row.
    auto *sideTabsButton = chromeButton(m_sideTabsAction);
    sideTabsButton->setCheckable(false);
    layout->addWidget(sideTabsButton, 0, Qt::AlignVCenter);

    m_tabBar = new TabBar;
    m_tabBar->setGroups(m_groups);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tabBar->installEventFilter(this);
    m_tabStrip->installEventFilter(this);
    layout->addWidget(m_tabBar, 1);

    auto *newTabButton = chromeButton(m_newTabAction);
    newTabButton->setToolTip(tr("New tab (%1)")
                                 .arg(m_newTabAction->shortcut().toString(QKeySequence::NativeText)));
    layout->addWidget(newTabButton, 0, Qt::AlignVCenter);

    if (m_isPrivate) {
        auto *badgeIcon = new QLabel;
        badgeIcon->setPixmap(Icons::icon(Icons::Private).pixmap(18, 18));
        auto *badge = new QLabel(tr("Private"));
        badge->setObjectName(QStringLiteral("privateBadge"));
        badge->setToolTip(tr("Nothing from this window is written to disk"));
        layout->addSpacing(8);
        layout->addWidget(badgeIcon);
        layout->addWidget(badge);
    }

    // The window's own title bar: the empty part of this row moves the window, and
    // the window buttons sit at its end (unless the system title bar is wanted).
    layout->addWidget(m_frame->controls());
    m_frame->addDragArea(m_tabStrip);
    m_frame->addDragArea(m_tabBar, [this](const QPoint &position) {
        return m_tabBar->tabAt(position) < 0;
    });

    connect(m_tabBar, &QTabBar::currentChanged, this, &BrowserWindow::onCurrentTabChanged);
    connect(m_tabBar, &QTabBar::currentChanged, this, &BrowserWindow::syncSideTabs);
    connect(m_tabBar, &QTabBar::tabMoved, this, &BrowserWindow::syncSideTabs);
    connect(m_tabBar, &TabBar::tabsChanged, this, &BrowserWindow::syncSideTabs);
    connect(m_tabBar, &TabBar::dragFinished, this, &BrowserWindow::onTabDragFinished);
    connect(m_tabBar, &TabBar::groupChipClicked, this, &BrowserWindow::toggleGroupCollapsed);
    connect(m_tabBar, &TabBar::groupMenuRequested, this, &BrowserWindow::showGroupMenu);
    connect(m_tabBar, &QTabBar::customContextMenuRequested, this,
            &BrowserWindow::showTabContextMenu);
    connect(m_tabBar, &QTabBar::tabBarDoubleClicked, this, [this](int index) {
        if (index < 0)
            newTab();
    });

    return m_tabStrip;
}

QWidget *BrowserWindow::createNavigationBar()
{
    m_navBar = new QWidget;
    m_navBar->setObjectName(QStringLiteral("navBar"));
    m_navBar->setAttribute(Qt::WA_StyledBackground);

    auto *layout = new QHBoxLayout(m_navBar);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);

    layout->addWidget(chromeButton(m_backAction));
    layout->addWidget(chromeButton(m_forwardAction));
    m_reloadStopButton = chromeButton(m_reloadAction);
    layout->addWidget(m_reloadStopButton);
    m_homeButton = chromeButton(m_homeAction);
    layout->addWidget(m_homeButton);
    layout->addSpacing(4);

    m_omnibox = new QLineEdit;
    m_omnibox->setObjectName(QStringLiteral("omnibox"));
    m_omnibox->setPlaceholderText(tr("Search with DuckDuckGo or enter address"));
    m_omnibox->setClearButtonEnabled(false);
    m_omnibox->setMinimumWidth(220);
    m_omnibox->installEventFilter(this);
    m_omnibox->setAccessibleName(tr("Address and search bar"));

    m_securityAction = m_omnibox->addAction(Icons::icon(Icons::Search), QLineEdit::LeadingPosition);
    connect(m_securityAction, &QAction::triggered, this, [this] {
        QMenu menu(this);
        menu.addAction(m_securityAction->icon(), m_securityAction->toolTip())->setEnabled(false);
        if (WebView *view = currentView()) {
            const QString host = view->url().host();
            if (!host.isEmpty() && m_services.adBlocker) {
                menu.addSeparator();
                const bool paused = m_services.adBlocker->isPausedFor(host);
                QAction *pause = menu.addAction(Icons::icon(Icons::Shield),
                                                paused ? tr("Resume blocking on %1").arg(host)
                                                       : tr("Pause blocking on %1").arg(host));
                connect(pause, &QAction::triggered, this, [this, host, paused] {
                    QStringList hosts = m_services.adBlocker->pausedHosts();
                    hosts.removeAll(host);
                    if (!paused)
                        hosts.append(host);
                    m_services.adBlocker->setPausedHosts(hosts);
                    updateBlockingIndicator();
                    if (WebView *current = currentView())
                        current->reload();
                });
            }
        }
        if (WebView *view = currentView(); view && m_services.profile) {
            const QUrl origin = view->url().adjusted(QUrl::RemovePath | QUrl::RemoveQuery
                                                     | QUrl::RemoveFragment);
            bool first = true;
            for (const QWebEnginePermission &permission : m_services.profile->listPermissionsForOrigin(origin)) {
                if (permission.state() == QWebEnginePermission::State::Ask)
                    continue;
                if (first)
                    menu.addSeparator();
                first = false;
                QAction *reset = menu.addAction(tr("%1: %2 — ask again")
                                                    .arg(SitePermissions::name(permission.permissionType()),
                                                         SitePermissions::stateName(permission.state())));
                connect(reset, &QAction::triggered, this, [permission] { permission.reset(); });
            }
            menu.addSeparator();
            connect(menu.addAction(tr("Site permissions…")), &QAction::triggered, this,
                    &BrowserWindow::showSitePermissions);
        }
        menu.exec(m_omnibox->mapToGlobal(QPoint(0, m_omnibox->height())));
    });

    m_bookmarkStarAction = m_omnibox->addAction(Icons::icon(Icons::Star),
                                                QLineEdit::TrailingPosition);
    m_bookmarkStarAction->setCheckable(true);
    m_bookmarkStarAction->setToolTip(tr("Bookmark this page (%1)")
                                         .arg(m_bookmarkPageAction->shortcut()
                                                  .toString(QKeySequence::NativeText)));
    connect(m_bookmarkStarAction, &QAction::triggered, this, [this] {
        // The action toggles itself on click; the bookmark state is the truth.
        updateBookmarkStar();
        toggleBookmarkForCurrentPage();
    });

    // Shown when the site has saved logins: pick one to fill it in.
    m_loginsAction = m_omnibox->addAction(Icons::icon(Icons::Key), QLineEdit::TrailingPosition);
    m_loginsAction->setToolTip(tr("Fill in a saved login"));
    m_loginsAction->setVisible(false);
    connect(m_loginsAction, &QAction::triggered, this, &BrowserWindow::showLoginsMenu);

    m_completionModel = new QStandardItemModel(this);
    m_completer = new QCompleter(m_completionModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCompletionRole(Qt::UserRole);
    m_completer->setMaxVisibleItems(8);
    m_omnibox->setCompleter(m_completer);
    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated), this,
            [this](const QString &text) {
                // Keyboard selection also triggers returnPressed; only navigate once.
                QTimer::singleShot(0, this, [this, text] {
                    if (!m_suppressCompleterNavigation)
                        navigate(text);
                });
            });

    connect(m_omnibox, &QLineEdit::returnPressed, this, &BrowserWindow::onOmniboxReturnPressed);
    connect(m_omnibox, &QLineEdit::textEdited, this, [this] {
        m_omniboxEdited = true;
        m_securityAction->setIcon(Icons::icon(Icons::Search));
    });

    layout->addWidget(m_omnibox, 1);
    layout->addSpacing(4);

    m_zoomButton = new QToolButton;
    m_zoomButton->setObjectName(QStringLiteral("zoomButton"));
    m_zoomButton->setFocusPolicy(Qt::NoFocus);
    m_zoomButton->setToolTip(tr("Reset zoom"));
    m_zoomButton->hide();
    connect(m_zoomButton, &QToolButton::clicked, this, [this] { setZoom(1.0); });
    layout->addWidget(m_zoomButton);

    m_blockingButton = new QToolButton;
    m_blockingButton->setObjectName(QStringLiteral("shieldButton"));
    m_blockingButton->setAccessibleName(tr("Ad blocking"));
    m_blockingButton->setPopupMode(QToolButton::InstantPopup);
    m_blockingButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_blockingButton->setIconSize(QSize(18, 18));
    m_blockingButton->setFocusPolicy(Qt::NoFocus);
    m_blockingMenu = new QMenu(this);
    m_blockingButton->setMenu(m_blockingMenu);
    connect(m_blockingMenu, &QMenu::aboutToShow, this, &BrowserWindow::populateBlockingMenu);
    layout->addWidget(m_blockingButton);

    // Chrome extensions: their popups, and the way to manage them.
    m_extensionsButton = new QToolButton;
    m_extensionsButton->setIcon(Icons::icon(Icons::Extensions));
    m_extensionsButton->setIconSize(QSize(18, 18));
    m_extensionsButton->setToolTip(tr("Extensions"));
    m_extensionsButton->setPopupMode(QToolButton::InstantPopup);
    m_extensionsButton->setFocusPolicy(Qt::NoFocus);
    m_extensionsMenu = new QMenu(this);
    m_extensionsButton->setMenu(m_extensionsMenu);
    connect(m_extensionsMenu, &QMenu::aboutToShow, this, &BrowserWindow::populateExtensionsMenu);
    m_extensionsButton->setVisible(ChromeExtensions::isSupported() && m_services.extensions);
    layout->addWidget(m_extensionsButton);

    m_downloadsButton = chromeButton(m_downloadsAction);
    layout->addWidget(m_downloadsButton);

    m_menuButton = new QToolButton;
    m_menuButton->setIcon(Icons::icon(Icons::Menu));
    m_menuButton->setIconSize(QSize(18, 18));
    m_menuButton->setToolTip(tr("Open menu"));
    m_menuButton->setAccessibleName(tr("Main menu"));
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    m_menuButton->setFocusPolicy(Qt::NoFocus);
    m_menuButton->setMenu(createAppMenu());
    layout->addWidget(m_menuButton);

    // With the tabs on the side this row is the title bar: the window buttons move
    // here and its empty part moves the window.
    m_frame->addDragArea(m_navBar, [this](const QPoint &) { return m_tabsOnSide; });

    return m_navBar;
}

void BrowserWindow::createBookmarksBar()
{
    m_bookmarksBar = new QToolBar(tr("Bookmarks"));
    m_bookmarksBar->setObjectName(QStringLiteral("bookmarksBar"));
    m_bookmarksBar->setMovable(false);
    m_bookmarksBar->setFloatable(false);
    m_bookmarksBar->setIconSize(QSize(16, 16));
    m_bookmarksBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_bookmarksBar->setContextMenuPolicy(Qt::PreventContextMenu);
}

QWidget *BrowserWindow::createFindBar()
{
    m_findBar = new QWidget;
    m_findBar->setObjectName(QStringLiteral("findBar"));
    m_findBar->setAttribute(Qt::WA_StyledBackground);
    m_findBar->hide();

    auto *layout = new QHBoxLayout(m_findBar);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(4);

    m_findInput = new QLineEdit;
    m_findInput->setPlaceholderText(tr("Find in page"));
    m_findInput->addAction(Icons::icon(Icons::Search), QLineEdit::LeadingPosition);
    m_findInput->installEventFilter(this);
    layout->addWidget(m_findInput);

    auto *previousButton = new QToolButton;
    previousButton->setIcon(Icons::icon(Icons::ChevronUp));
    previousButton->setToolTip(tr("Previous match (Shift+Enter)"));
    connect(previousButton, &QToolButton::clicked, this, [this] { findNext(true); });
    layout->addWidget(previousButton);

    auto *nextButton = new QToolButton;
    nextButton->setIcon(Icons::icon(Icons::ChevronDown));
    nextButton->setToolTip(tr("Next match (Enter)"));
    connect(nextButton, &QToolButton::clicked, this, [this] { findNext(false); });
    layout->addWidget(nextButton);

    m_findStatus = new QLabel;
    m_findStatus->setObjectName(QStringLiteral("findStatus"));
    layout->addWidget(m_findStatus);
    layout->addStretch(1);

    auto *closeButton = new QToolButton;
    closeButton->setIcon(Icons::icon(Icons::Close));
    closeButton->setToolTip(tr("Close the find bar (Esc)"));
    connect(closeButton, &QToolButton::clicked, this, &BrowserWindow::hideFindBar);
    layout->addWidget(closeButton);

    connect(m_findInput, &QLineEdit::textChanged, this, [this] { findNext(false); });

    return m_findBar;
}

QMenu *BrowserWindow::createAppMenu()
{
    auto *menu = new QMenu(this);
    menu->addAction(m_newTabAction);
    menu->addAction(m_newWindowAction);
    menu->addAction(m_newPrivateWindowAction);
    menu->addSeparator();

    QMenu *bookmarks = menu->addMenu(Icons::icon(Icons::Bookmarks), tr("Bookmarks"));
    connect(bookmarks, &QMenu::aboutToShow, this, [this, bookmarks] {
        bookmarks->clear();
        bookmarks->addAction(m_bookmarkPageAction);
        bookmarks->addAction(m_bookmarksBarAction);
        bookmarks->addAction(m_bookmarksAction);
        if (!m_services.bookmarkManager)
            return;
        bool separated = false;
        for (const Bookmark &bookmark : m_services.bookmarkManager->bookmarks()) {
            if (!bookmark.url.isValid() || bookmark.url.isEmpty())
                continue;
            if (!separated) {
                bookmarks->addSeparator();
                separated = true;
            }
            const QUrl url = bookmark.url;
            QAction *action = bookmarks->addAction(
                Icons::icon(Icons::Globe),
                bookmark.title.isEmpty() ? url.toDisplayString() : bookmark.title);
            action->setToolTip(url.toDisplayString());
            connect(action, &QAction::triggered, this, [this, url] {
                if (WebView *view = currentView())
                    loadUrl(view, url);
            });
        }
    });

    menu->addAction(m_historyAction);
    menu->addAction(m_downloadsAction);
    menu->addSeparator();

    // Zoom controls in one row, the way most browsers lay them out.
    auto *zoomRow = new QWidget(menu);
    auto *zoomLayout = new QHBoxLayout(zoomRow);
    zoomLayout->setContentsMargins(12, 2, 6, 2);
    auto *zoomLabel = new QLabel(tr("Zoom"), zoomRow);
    zoomLayout->addWidget(zoomLabel);
    zoomLayout->addStretch(1);
    auto *zoomOut = new QToolButton(zoomRow);
    zoomOut->setText(QStringLiteral("−"));
    zoomOut->setAutoRaise(true);
    zoomOut->setToolTip(m_zoomOutAction->text());
    connect(zoomOut, &QToolButton::clicked, m_zoomOutAction, &QAction::trigger);
    m_zoomMenuLabel = new QLabel(formatPercent(1.0), zoomRow);
    m_zoomMenuLabel->setMinimumWidth(48);
    m_zoomMenuLabel->setAlignment(Qt::AlignCenter);
    auto *zoomIn = new QToolButton(zoomRow);
    zoomIn->setText(QStringLiteral("+"));
    zoomIn->setAutoRaise(true);
    zoomIn->setToolTip(m_zoomInAction->text());
    connect(zoomIn, &QToolButton::clicked, m_zoomInAction, &QAction::trigger);
    auto *fullScreen = new QToolButton(zoomRow);
    fullScreen->setText(QStringLiteral("⛶"));
    fullScreen->setAutoRaise(true);
    fullScreen->setToolTip(m_fullScreenAction->text());
    connect(fullScreen, &QToolButton::clicked, this, [this, menu] {
        menu->close();
        m_fullScreenAction->trigger();
    });
    zoomLayout->addWidget(zoomOut);
    zoomLayout->addWidget(m_zoomMenuLabel);
    zoomLayout->addWidget(zoomIn);
    zoomLayout->addSpacing(6);
    zoomLayout->addWidget(fullScreen);
    auto *zoomAction = new QWidgetAction(menu);
    zoomAction->setDefaultWidget(zoomRow);
    menu->addAction(zoomAction);

    menu->addAction(m_findAction);
    menu->addSeparator();

    QMenu *themeMenu = menu->addMenu(tr("Theme"));
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    for (const Theme::Mode mode : Theme::modes()) {
        QAction *action = themeMenu->addAction(Theme::modeLabel(mode));
        action->setCheckable(true);
        action->setData(static_cast<int>(mode));
        connect(action, &QAction::triggered, this, [this, mode] { applyTheme(mode); });
        m_themeGroup->addAction(action);
    }
    syncThemeMenu();

    menu->addAction(m_extensionsAction);
    menu->addAction(m_settingsAction);
    menu->addSeparator();
    menu->addAction(m_sideTabsAction);
    menu->addAction(m_menuBarAction);

    QMenu *help = menu->addMenu(tr("Help"));
    help->addAction(m_shortcutsAction);
    help->addAction(m_aboutAction);
    QAction *aboutQt = help->addAction(tr("About Qt"));
    connect(aboutQt, &QAction::triggered, this, [] { QApplication::aboutQt(); });

    menu->addSeparator();
    menu->addAction(m_quitAction);

    connect(menu, &QMenu::aboutToShow, this, &BrowserWindow::updateZoomIndicator);
    return menu;
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
    fileMenu->addAction(m_quitAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_backAction);
    viewMenu->addAction(m_forwardAction);
    viewMenu->addAction(m_reloadAction);
    viewMenu->addAction(m_stopAction);
    viewMenu->addAction(m_homeAction);
    viewMenu->addAction(m_focusAddressAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_nextTabAction);
    viewMenu->addAction(m_previousTabAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_findAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_zoomResetAction);
    viewMenu->addAction(m_fullScreenAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_bookmarksBarAction);
    viewMenu->addAction(m_sideTabsAction);
    viewMenu->addAction(m_menuBarAction);
    QMenu *themeMenu = viewMenu->addMenu(tr("Theme"));
    themeMenu->addActions(m_themeGroup->actions());

    QMenu *bookmarksMenu = menuBar()->addMenu(tr("&Bookmarks"));
    bookmarksMenu->addAction(m_bookmarkPageAction);
    bookmarksMenu->addAction(m_bookmarksAction);

    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(m_historyAction);
    toolsMenu->addAction(m_downloadsAction);
    toolsMenu->addAction(m_extensionsAction);
    toolsMenu->addSeparator();
    QAction *blockingAction = toolsMenu->addAction(Icons::icon(Icons::Shield), tr("Ad Blocking"));
    connect(blockingAction, &QAction::triggered, this, [this] { m_blockingButton->showMenu(); });
    toolsMenu->addSeparator();
    toolsMenu->addAction(m_settingsAction);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_shortcutsAction);
    helpMenu->addAction(m_aboutAction);
    QAction *aboutQtAction = helpMenu->addAction(tr("About Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    connect(aboutQtAction, &QAction::triggered, this, [] { QApplication::aboutQt(); });
}

void BrowserWindow::createTabShortcuts()
{
    for (int number = 1; number <= 9; ++number) {
        auto *shortcut = new QShortcut(QKeySequence(Qt::CTRL | (Qt::Key_1 + (number - 1))), this);
        connect(shortcut, &QShortcut::activated, this, [this, number] {
            // Ctrl+9 always means the last tab, as in other browsers.
            selectTabByIndex(number == 9 ? m_tabBar->count() - 1 : number - 1);
        });
    }
}

// ---- Preferences --------------------------------------------------------------

void BrowserWindow::applyPreferences()
{
    Preferences *preferences = Preferences::instance();
    m_resolver = UrlResolver(preferences->searchTemplate(), preferences->homePage());

    QString engine = tr("the web");
    for (const SearchEngine &candidate : Preferences::searchEngines()) {
        if (candidate.searchTemplate == preferences->searchTemplate())
            engine = candidate.name;
    }
    m_omnibox->setPlaceholderText(tr("Search %1 or enter address").arg(engine));

    m_homeButton->setVisible(preferences->showHomeButton());
    m_bookmarksBar->setVisible(preferences->showBookmarksBar());
    m_bookmarksBarAction->setChecked(preferences->showBookmarksBar());
#ifdef Q_OS_MACOS
    // The macOS menu bar belongs to the system, at the top of the screen, and is
    // always there; there is nothing to show or hide.
    m_menuBarAction->setVisible(false);
#else
    menuBar()->setVisible(preferences->showMenuBar());
    m_menuBarAction->setChecked(preferences->showMenuBar());
#endif
    m_frame->setCustomTitleBar(!preferences->useSystemTitleBar());
    setTabsOnSide(preferences->sideTabs());
    m_sideTabs->setCollapsed(preferences->sideTabsCollapsed());

    syncThemeMenu();
    // The groups' colours follow the theme.
    refreshGroups();
    updateBlockingIndicator();
}

void BrowserWindow::applyTheme(Theme::Mode mode)
{
    Theme::storeMode(mode);
    Theme::apply(mode);
    Preferences::instance()->notifyChanged();
}

void BrowserWindow::syncThemeMenu()
{
    if (!m_themeGroup)
        return;
    const int current = static_cast<int>(Theme::storedMode());
    for (QAction *action : m_themeGroup->actions())
        action->setChecked(action->data().toInt() == current);
}

void BrowserWindow::applyDataSaver(WebView *view) const
{
    if (!view || !m_services.dataSaverManager)
        return;

    const DataSaverManager::Mode mode = m_services.dataSaverManager->mode();
    QWebEngineSettings *settings = view->page()->settings();
    settings->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,
                           mode == DataSaverManager::Mode::Off);
    settings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, false);
    settings->setAttribute(QWebEngineSettings::AutoLoadImages,
                           mode != DataSaverManager::Mode::Aggressive);
}

// ---- Dialogs ------------------------------------------------------------------

void BrowserWindow::showExtensionsDialog()
{
    if (!m_services.userScripts)
        return;

    ExtensionsDialog dialog(m_services.userScripts, m_services.extensions, this);
    connect(&dialog, &ExtensionsDialog::webStoreRequested, this, [this] {
        newTab(QUrl(QStringLiteral("https://chromewebstore.google.com/category/extensions")));
    });
    dialog.exec();
}

void BrowserWindow::showSettings()
{
    SettingsDialog dialog(
        m_services.adBlocker,
        m_services.bookmarkManager,
        m_services.cookieManager,
        m_services.dataSaverManager,
        m_services.downloadManager,
        m_services.historyManager,
        m_services.userScripts,
        this);
    dialog.setProfile(m_services.profile);
    connect(&dialog, &SettingsDialog::extensionsRequested, this,
            &BrowserWindow::showExtensionsDialog);
    connect(&dialog, &SettingsDialog::savedLoginsRequested, this, &BrowserWindow::showSavedLogins);
    connect(&dialog, &SettingsDialog::sitePermissionsRequested, this,
            &BrowserWindow::showSitePermissions);
    dialog.exec();
}

void BrowserWindow::showDownloads()
{
    if (!m_services.downloadManager)
        return;

    if (!m_downloadsDialog)
        m_downloadsDialog = new DownloadsDialog(m_services.downloadManager, this);
    m_downloadsDialog->show();
    m_downloadsDialog->raise();
    m_downloadsDialog->activateWindow();
}

void BrowserWindow::showHistory()
{
    if (!m_services.historyManager)
        return;

    if (!m_historyDialog) {
        m_historyDialog = new HistoryDialog(m_services.historyManager, this);
        connect(m_historyDialog, &HistoryDialog::openRequested, this,
                [this](const QUrl &url) { newTab(url); });
    }
    m_historyDialog->show();
    m_historyDialog->raise();
    m_historyDialog->activateWindow();
}

void BrowserWindow::showBookmarks()
{
    if (!m_services.bookmarkManager)
        return;

    BookmarksDialog dialog(m_services.bookmarkManager, this);
    connect(&dialog, &BookmarksDialog::openRequested, this,
            [this](const QUrl &url) { newTab(url); });
    dialog.exec();
}

void BrowserWindow::showShortcuts()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Keyboard Shortcuts"));
    dialog.resize(520, 560);

    auto *layout = new QVBoxLayout(&dialog);
    auto *tree = new QTreeWidget(&dialog);
    tree->setColumnCount(2);
    tree->setHeaderLabels({tr("Action"), tr("Shortcut")});
    tree->setRootIsDecorated(false);
    tree->setAlternatingRowColors(false);

    const auto actions = findChildren<QAction *>();
    for (QAction *action : actions) {
        if (action->shortcut().isEmpty() || action->text().isEmpty())
            continue;
        auto *item = new QTreeWidgetItem(tree);
        QString text = action->text();
        text.remove(QLatin1Char('&'));
        text.remove(QStringLiteral("…"));
        item->setText(0, text);
        item->setIcon(0, action->icon());
        item->setText(1, action->shortcut().toString(QKeySequence::NativeText));
    }
    auto *tabs = new QTreeWidgetItem(tree);
    tabs->setText(0, tr("Go to tab 1–8 / last tab"));
    tabs->setText(1, tr("%1+1 … %1+9").arg(Shortcuts::primaryModifier()));

    tree->sortItems(0, Qt::AscendingOrder);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    layout->addWidget(tree);
    layout->addWidget(buttons);
    dialog.exec();
}

// ---- Blocking -----------------------------------------------------------------

void BrowserWindow::updateBlockingIndicator()
{
    if (!m_blockingButton)
        return;

    AdBlocker *blocker = m_services.adBlocker;
    if (!blocker) {
        m_blockingButton->hide();
        return;
    }

    const bool active = blocker->isEnabled() && blocker->ruleCount() > 0;
    QIcon icon = Icons::icon(Icons::Shield);
    m_blockingButton->setIcon(icon);
    m_blockingButton->setProperty("active", active);
    m_blockingButton->style()->unpolish(m_blockingButton);
    m_blockingButton->style()->polish(m_blockingButton);

    if (blocker->ruleCount() == 0) {
        m_blockingButton->setText(QString());
        m_blockingButton->setToolTip(
            tr("No filter list is loaded. Put rules in %1").arg(blocker->listPath()));
        return;
    }

    if (!blocker->isEnabled()) {
        m_blockingButton->setText(tr("Off"));
        m_blockingButton->setToolTip(
            tr("Ad blocking is off. %1 rules are loaded.").arg(blocker->ruleCount()));
        return;
    }

    const int blocked = blocker->blockedCount();
    m_blockingButton->setText(blocked > 0 ? QLocale().toString(blocked) : QString());
    m_blockingButton->setToolTip(tr("%1 requests blocked with %2 rules. Click for options.")
                                     .arg(blocked)
                                     .arg(blocker->ruleCount()));
}

void BrowserWindow::populateBlockingMenu()
{
    if (!m_services.adBlocker || !m_blockingMenu)
        return;

    m_blockingMenu->clear();
    AdBlocker *blocker = m_services.adBlocker;

    QAction *summary = m_blockingMenu->addAction(
        tr("%1 blocked · %2 network rules · %3 cosmetic rules")
            .arg(blocker->blockedCount())
            .arg(blocker->ruleCount())
            .arg(blocker->cosmeticRuleCount()));
    summary->setEnabled(false);
    m_blockingMenu->addSeparator();

    if (WebView *view = currentView()) {
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
                if (WebView *current = currentView())
                    current->reload();
            });
        }
    }

    QAction *toggleAction = m_blockingMenu->addAction(
        blocker->isEnabled() ? tr("Turn off ad blocking") : tr("Turn on ad blocking"));
    connect(toggleAction, &QAction::triggered, this, [this] {
        Preferences::instance()->setBlockingEnabled(!m_services.adBlocker->isEnabled());
    });

    if (!blocker->lastListError().isEmpty()) {
        m_blockingMenu->addSeparator();
        m_blockingMenu->addAction(blocker->lastListError())->setEnabled(false);
    }
}

// ---- Bookmarks ----------------------------------------------------------------

QString BrowserWindow::bookmarkIdForUrl(const QUrl &url) const
{
    if (!m_services.bookmarkManager || !url.isValid() || url.isEmpty())
        return QString();
    for (const Bookmark &bookmark : m_services.bookmarkManager->bookmarks()) {
        if (bookmark.url == url)
            return bookmark.id;
    }
    return QString();
}

void BrowserWindow::rebuildBookmarksBar()
{
    if (!m_bookmarksBar)
        return;

    m_bookmarksBar->clear();

    QList<Bookmark> bookmarks;
    if (m_services.bookmarkManager) {
        for (const Bookmark &bookmark : m_services.bookmarkManager->bookmarks()) {
            if (bookmark.url.isValid() && !bookmark.url.isEmpty())
                bookmarks.prepend(bookmark); // oldest first, like every other bookmarks bar
        }
    }

    if (bookmarks.isEmpty()) {
        auto *hint = new QLabel(tr("Bookmark pages with %1 and they will show up here.")
                                    .arg(m_bookmarkPageAction->shortcut()
                                             .toString(QKeySequence::NativeText)));
        m_bookmarksBar->addWidget(hint);
        return;
    }

    for (const Bookmark &bookmark : bookmarks) {
        const QUrl url = bookmark.url;
        const QString id = bookmark.id;
        QString title = bookmark.title.isEmpty() ? url.host() : bookmark.title;
        if (title.size() > 28)
            title = title.left(26) + QStringLiteral("…");

        auto *button = new QToolButton;
        button->setText(title);
        button->setIcon(Icons::icon(Icons::Globe));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(QStringLiteral("%1\n%2").arg(bookmark.title, url.toDisplayString()));
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QToolButton::clicked, this, [this, url] {
            if (QGuiApplication::keyboardModifiers() & Qt::ControlModifier) {
                newTab(url, true);
            } else if (WebView *view = currentView()) {
                loadUrl(view, url);
            }
        });
        connect(button, &QToolButton::customContextMenuRequested, this,
                [this, button, url, id](const QPoint &pos) {
                    QMenu menu(this);
                    connect(menu.addAction(tr("Open")), &QAction::triggered, this, [this, url] {
                        if (WebView *view = currentView())
                            loadUrl(view, url);
                    });
                    connect(menu.addAction(tr("Open in New Tab")), &QAction::triggered, this,
                            [this, url] { newTab(url, true); });
                    menu.addSeparator();
                    connect(menu.addAction(tr("Edit…")), &QAction::triggered, this, [this, id] {
                        BookmarkEditDialog::editBookmark(m_services.bookmarkManager, id, this);
                    });
                    connect(menu.addAction(Icons::icon(Icons::Trash), tr("Delete")),
                            &QAction::triggered, this,
                            [this, id] { m_services.bookmarkManager->removeBookmark(id); });
                    menu.exec(button->mapToGlobal(pos));
                });
        m_bookmarksBar->addWidget(button);
    }
}

void BrowserWindow::toggleBookmarkForCurrentPage()
{
    WebView *view = currentView();
    if (!view || !m_services.bookmarkManager)
        return;

    const QUrl url = view->url();
    if (!isWebScheme(url))
        return;

    const QString existing = bookmarkIdForUrl(url);
    if (!existing.isEmpty()) {
        BookmarkEditDialog::editBookmark(m_services.bookmarkManager, existing, this);
        return;
    }

    Bookmark bookmark;
    bookmark.title = view->title().trimmed().isEmpty() ? url.host() : view->title().trimmed();
    bookmark.url = url;
    m_services.bookmarkManager->addBookmark(bookmark);
    showToast(tr("Bookmarked “%1”").arg(bookmark.title), 2000);
}

void BrowserWindow::updateBookmarkStar()
{
    WebView *view = currentView();
    const bool bookmarkable = view && isWebScheme(view->url()) && m_services.bookmarkManager;
    m_bookmarkStarAction->setVisible(bookmarkable);
    m_bookmarkPageAction->setEnabled(bookmarkable);
    const bool bookmarked = bookmarkable && !bookmarkIdForUrl(view->url()).isEmpty();
    m_bookmarkStarAction->setChecked(bookmarked);
    m_bookmarkStarAction->setToolTip(bookmarked ? tr("Edit bookmark")
                                                : tr("Bookmark this page (%1)")
                                                      .arg(m_bookmarkPageAction->shortcut()
                                                               .toString(QKeySequence::NativeText)));
}

// ---- Session ------------------------------------------------------------------

void BrowserWindow::restoreSession()
{
    ServiceRegistry &shared = registry();
    if (m_isPrivate || shared.sessionRestored) {
        newTab();
        return;
    }
    shared.sessionRestored = true;

    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    if (!Preferences::instance()->restoreSession()) {
        newTab();
        return;
    }

    const QStringList urls =
        settings.value(QStringLiteral("session/urls")).toStringList().mid(0, kMaximumRestoredTabs);

    if (urls.isEmpty()) {
        newTab();
        return;
    }

    const int current = qBound(0, settings.value(QStringLiteral("session/currentTab")).toInt(),
                               int(urls.size()) - 1);
    for (const QString &url : urls)
        newTab(QUrl(url));

    const QJsonObject saved =
        QJsonDocument::fromJson(settings.value(QStringLiteral("session/groups")).toString().toUtf8())
            .object();
    const QJsonArray groupList = saved.value(QStringLiteral("groups")).toArray();
    const QJsonArray tabGroups = saved.value(QStringLiteral("tabs")).toArray();
    QHash<int, int> ids; // place in the saved list -> new group
    for (int index = 0; index < tabGroups.size() && index < m_tabBar->count(); ++index) {
        const int place = tabGroups.at(index).toInt(-1);
        if (place < 0 || place >= groupList.size())
            continue;
        if (!ids.contains(place)) {
            const QJsonObject group = groupList.at(place).toObject();
            ids.insert(place, m_groups->create(group.value(QStringLiteral("title")).toString(),
                                               group.value(QStringLiteral("color")).toInt()));
        }
        m_groups->setGroupOf(viewAt(index), ids.value(place));
    }
    for (auto it = ids.cbegin(); it != ids.cend(); ++it) {
        m_groups->setCollapsed(it.value(),
                               groupList.at(it.key()).toObject().value(QStringLiteral("collapsed")).toBool());
    }
    m_groups->arrange(m_tabBar);
    refreshGroups();
    m_tabBar->setCurrentIndex(current);
}

void BrowserWindow::saveSession() const
{
    if (m_isPrivate)
        return;

    QSettings settings;

    QStringList urls;
    urls.reserve(m_tabBar->count());
    // Each saved tab's group, as a place in the list of groups; -1 for none.
    const QList<TabGroup> groups = m_groups->groups();
    QJsonArray tabGroups;
    for (int index = 0; index < m_tabBar->count(); ++index) {
        const WebView *view = viewAt(index);
        if (view && view->url().isValid() && !view->url().isEmpty()) {
            urls.append(view->url().toString());
            const int group = m_groups->groupOf(view);
            int place = -1;
            for (int i = 0; i < groups.size(); ++i) {
                if (groups.at(i).id == group)
                    place = i;
            }
            tabGroups.append(place);
        }
    }
    QJsonArray groupList;
    for (const TabGroup &group : groups) {
        groupList.append(QJsonObject{{QStringLiteral("title"), group.title},
                                     {QStringLiteral("color"), group.color},
                                     {QStringLiteral("collapsed"), group.collapsed}});
    }

    settings.setValue(QStringLiteral("session/urls"), urls);
    settings.setValue(QStringLiteral("session/groups"),
                      QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("groups"), groupList},
                                                                  {QStringLiteral("tabs"), tabGroups}})
                                            .toJson(QJsonDocument::Compact)));
    settings.setValue(QStringLiteral("session/currentTab"), m_tabBar->currentIndex());
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
}

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    if (!registry().quitting)
        saveSession();
    QMainWindow::closeEvent(event);
}

// ---- Tabs ---------------------------------------------------------------------

WebView *BrowserWindow::currentView() const
{
    return viewAt(m_tabBar ? m_tabBar->currentIndex() : -1);
}

WebView *BrowserWindow::viewAt(int index) const
{
    if (!m_tabBar || index < 0 || index >= m_tabBar->count())
        return nullptr;
    return qobject_cast<WebView *>(m_tabBar->tabData(index).value<QObject *>());
}

int BrowserWindow::indexOfView(const QWebEngineView *view) const
{
    for (int index = 0; index < m_tabBar->count(); ++index) {
        if (viewAt(index) == view)
            return index;
    }
    return -1;
}

WebView *BrowserWindow::addTab(bool background)
{
    auto *view = new WebView(m_services.profile);
    view->setZoomFactor(1.0);
    applyDataSaver(view);
    connectView(view);
    m_stack->addWidget(view);

    // Links opened in the background land next to the tab they came from.
    const int current = m_tabBar->currentIndex();
    const int insertAt = current < 0 ? 0 : (background ? current + 1 : m_tabBar->count());
    const int index = m_tabBar->insertTab(insertAt, Icons::icon(Icons::Globe), tr("New Tab"));
    m_tabBar->setTabData(index, QVariant::fromValue(static_cast<QObject *>(view)));
    m_tabBar->setLabel(index, {tr("New Tab"), Icons::icon(Icons::Globe), tr("New Tab")});

    auto *closeButton = new QToolButton;
    closeButton->setObjectName(QStringLiteral("tabClose"));
    closeButton->setIcon(Icons::icon(Icons::Close));
    closeButton->setIconSize(QSize(12, 12));
    closeButton->setFixedSize(18, 18);
    closeButton->setAutoRaise(true);
    closeButton->setFocusPolicy(Qt::NoFocus);
    closeButton->setToolTip(tr("Close tab"));
    connect(closeButton, &QToolButton::clicked, this, [this, view] { closeTab(indexOfView(view)); });
    m_tabBar->setTabButton(index, QTabBar::RightSide, closeButton);

    // A link opened in the background from a grouped tab joins its group, as in
    // other browsers; any other new tab stays out of the groups it lands among.
    if (background && current >= 0)
        m_groups->setGroupOf(view, m_groups->groupOf(tabObject(m_tabBar, current)));
    m_groups->arrange(m_tabBar);
    refreshGroups();

    if (!background || m_tabBar->count() == 1)
        m_tabBar->setCurrentIndex(indexOfView(view));

    return view;
}

WebView *BrowserWindow::newTab(const QUrl &url, bool background)
{
    WebView *view = addTab(background);
    loadUrl(view, url.isEmpty() ? m_resolver.homeUrl() : url);
    return view;
}

void BrowserWindow::openInNewTab(const QString &text)
{
    newTab(m_resolver.resolve(text));
}

BrowserWindow *BrowserWindow::openWindow(bool isPrivate, const QStringList &texts)
{
    BrowserWindow *window = newWindow(isPrivate);
    if (!texts.isEmpty()) {
        // Replace the new window's start tab with the requested pages.
        const int startTabs = window->m_tabBar->count();
        for (const QString &text : texts)
            window->openInNewTab(text);
        for (int index = startTabs - 1; index >= 0; --index)
            window->closeTab(index);
    }
    return window;
}

BrowserWindow *BrowserWindow::newWindow(bool isPrivate)
{
    ServiceRegistry &shared = registry();
    const QString filterList = m_services.adBlocker ? m_services.adBlocker->listPath() : QString();

    ProfileServices services;
    if (isPrivate) {
        if (shared.privateServices.profile) {
            services = shared.privateServices;
        } else {
            services = createProfileServices(qApp, true, filterList);
            // Bookmarks are one list on disk; two managers would overwrite each other.
            if (shared.normal.bookmarkManager) {
                services.bookmarkManager->deleteLater();
                services.bookmarkManager = shared.normal.bookmarkManager;
            }
        }
    } else {
        services = shared.normal.profile ? shared.normal
                                         : createProfileServices(qApp, false, filterList);
    }

    auto *window = new BrowserWindow(std::move(services));
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
    return window;
}

void BrowserWindow::closeTab(int index)
{
    WebView *view = viewAt(index);
    if (!view)
        return;

    const QUrl url = view->url();
    if (url.isValid() && !url.isEmpty()) {
        m_closedTabs.prepend(ClosedTab{index, url});
        while (m_closedTabs.size() > kMaximumClosedTabs)
            m_closedTabs.removeLast();
        m_reopenTabAction->setEnabled(true);
    }

    if (m_findView == view)
        m_findView = nullptr;

    if (m_tabBar->count() == 1) {
        close();
        return;
    }

    m_tabBar->releaseChip(index);
    m_tabBar->removeTab(index);
    m_stack->removeWidget(view);
    m_groups->forget(view);
    view->deleteLater();
    refreshGroups();
}

void BrowserWindow::closeOtherTabs(int index)
{
    WebView *keep = viewAt(index);
    if (!keep)
        return;
    for (int i = m_tabBar->count() - 1; i >= 0; --i) {
        if (viewAt(i) != keep)
            closeTab(i);
    }
}

void BrowserWindow::duplicateTab(int index)
{
    if (WebView *view = viewAt(index))
        newTab(view->url());
}

void BrowserWindow::reopenClosedTab()
{
    if (m_closedTabs.isEmpty())
        return;

    const ClosedTab closed = m_closedTabs.takeFirst();
    m_reopenTabAction->setEnabled(!m_closedTabs.isEmpty());

    WebView *view = newTab(closed.url);
    const int from = indexOfView(view);
    const int to = qBound(0, closed.index, m_tabBar->count() - 1);
    if (from != to)
        m_tabBar->moveTab(from, to);
    m_groups->arrange(m_tabBar);
    refreshGroups();
    m_tabBar->setCurrentIndex(indexOfView(view));
}

void BrowserWindow::selectTabByOffset(int offset)
{
    if (m_tabBar->count() < 2)
        return;
    // Tabs in collapsed groups are skipped, as in other browsers.
    const int count = m_tabBar->count();
    int index = m_tabBar->currentIndex();
    for (int step = 0; step < count; ++step) {
        index = (index + offset + count) % count;
        if (!m_groups->isCollapsed(m_groups->groupOf(tabObject(m_tabBar, index))))
            break;
    }
    m_tabBar->setCurrentIndex(index);
}

void BrowserWindow::selectTabByIndex(int index)
{
    if (index >= 0 && index < m_tabBar->count())
        m_tabBar->setCurrentIndex(index);
}

void BrowserWindow::showTabContextMenu(const QPoint &pos)
{
    showTabMenu(m_tabBar->tabAt(pos), m_tabBar->mapToGlobal(pos));
}

void BrowserWindow::showTabMenu(int index, const QPoint &globalPosition)
{
    QMenu menu(this);
    menu.addAction(m_newTabAction);
    if (index >= 0) {
        menu.addSeparator();
        connect(menu.addAction(Icons::icon(Icons::Reload), tr("Reload")), &QAction::triggered,
                this, [this, index] { if (WebView *view = viewAt(index)) view->reload(); });
        connect(menu.addAction(tr("Duplicate")), &QAction::triggered, this,
                [this, index] { duplicateTab(index); });

        menu.addSeparator();
        QObject *tab = tabObject(m_tabBar, index);
        const int ownGroup = m_groups->groupOf(tab);
        connect(menu.addAction(tr("Add Tab to New Group")), &QAction::triggered, this,
                [this, index] { addTabToNewGroup(index); });
        const QList<TabGroup> groups = m_groups->groups();
        if (groups.size() > (ownGroup ? 1 : 0)) {
            QMenu *addTo = menu.addMenu(tr("Add Tab to Group"));
            for (const TabGroup &group : groups) {
                if (group.id == ownGroup)
                    continue;
                QPixmap swatch(12, 12);
                swatch.fill(Qt::transparent);
                QPainter painter(&swatch);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(Qt::NoPen);
                painter.setBrush(TabGroups::color(group.color, Theme::isDark()));
                painter.drawEllipse(QRectF(1, 1, 10, 10));
                painter.end();
                const QString name = group.title.isEmpty() ? tr("Unnamed group") : group.title;
                connect(addTo->addAction(QIcon(swatch), name), &QAction::triggered, this,
                        [this, tab, id = group.id] {
                            m_groups->setGroupOf(tab, id);
                            m_groups->arrange(m_tabBar);
                            refreshGroups();
                        });
            }
        }
        if (ownGroup) {
            connect(menu.addAction(tr("Remove from Group")), &QAction::triggered, this, [this, tab] {
                // Keeping the group together moves the tab out of its run of tabs.
                m_groups->setGroupOf(tab, 0);
                m_groups->arrange(m_tabBar);
                refreshGroups();
            });
        }

        menu.addSeparator();
        connect(menu.addAction(Icons::icon(Icons::Close), tr("Close Tab")), &QAction::triggered,
                this, [this, index] { closeTab(index); });
        QAction *others = menu.addAction(tr("Close Other Tabs"));
        others->setEnabled(m_tabBar->count() > 1);
        connect(others, &QAction::triggered, this, [this, index] { closeOtherTabs(index); });
    }
    menu.addAction(m_reopenTabAction);
    menu.addSeparator();
    menu.addAction(m_sideTabsAction);
    menu.exec(globalPosition);
}

void BrowserWindow::updateTabLabel(WebView *view)
{
    const int index = indexOfView(view);
    if (index < 0)
        return;

    const QUrl url = view->url();
    QString title = view->title().trimmed();
    if (isStartPage(url)) {
        title = tr("New Tab");
    } else if (url.scheme() == QLatin1String("about") && AboutPages::isKnown(url.toString())) {
        title = AboutPages::pageTitle(url.toString());
    } else if (title.isEmpty() || title == url.toString() || title == url.toDisplayString()) {
        if (url.scheme() == QLatin1String("about"))
            title = AboutPages::pageTitle(url.toString());
        else if (!url.host().isEmpty())
            title = url.host();
        else
            title = tr("New Tab");
    }

    const QString toolTip = url.isEmpty() ? title
                                          : QStringLiteral("%1\n%2").arg(title, url.toDisplayString());

    QIcon icon = view->icon();
    if (isStartPage(url) || url.scheme() == QLatin1String("about"))
        icon = Icons::icon(Icons::Penguin);
    else if (icon.isNull())
        icon = Icons::icon(Icons::Globe);
    m_tabBar->setLabel(index, {title, icon, toolTip});
    syncSideTabs();
}

// ---- Questions above the page --------------------------------------------------------

QWidget *BrowserWindow::createPromptBar()
{
    auto *bar = new QWidget;
    bar->setObjectName(QStringLiteral("promptBar"));
    bar->setAttribute(Qt::WA_StyledBackground);
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(12, 6, 6, 6);
    barLayout->setSpacing(10);
    m_promptIcon = new QLabel;
    barLayout->addWidget(m_promptIcon);
    m_promptText = new QLabel;
    m_promptText->setWordWrap(true);
    barLayout->addWidget(m_promptText, 1);
    m_promptButtons = new QWidget;
    auto *buttons = new QHBoxLayout(m_promptButtons);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(6);
    barLayout->addWidget(m_promptButtons);
    auto *dismiss = new QToolButton;
    dismiss->setIcon(Icons::icon(Icons::Close));
    dismiss->setAutoRaise(true);
    dismiss->setToolTip(tr("Not now"));
    connect(dismiss, &QToolButton::clicked, this, [this] { answerPrompt(-1); });
    barLayout->addWidget(dismiss);

    // A card that keeps clear of the window's edges, like the sandbox notice.
    m_promptBar = new QWidget;
    auto *holder = new QVBoxLayout(m_promptBar);
    holder->setContentsMargins(8, 6, 8, 2);
    holder->addWidget(bar);
    m_promptBar->hide();
    return m_promptBar;
}

void BrowserWindow::addPrompt(WebView *view, Prompt prompt)
{
    QList<Prompt> &queue = m_prompts[view];
    for (const Prompt &queued : std::as_const(queue)) {
        if (queued.key == prompt.key)
            return;
    }
    queue.append(std::move(prompt));
    if (view == currentView())
        showPrompt();
}

void BrowserWindow::dropPrompts(WebView *view, const QString &key)
{
    auto found = m_prompts.find(view);
    if (found == m_prompts.end())
        return;
    found->removeIf([&key](const Prompt &prompt) { return prompt.key == key; });
    if (view == currentView())
        showPrompt();
}

void BrowserWindow::showPrompt()
{
    if (!m_promptBar)
        return;
    const QList<Prompt> queue = m_prompts.value(currentView());
    if (queue.isEmpty()) {
        m_promptBar->hide();
        return;
    }
    const Prompt &prompt = queue.first();
    m_promptIcon->setPixmap(Icons::icon(prompt.icon).pixmap(18, 18));
    m_promptText->setText(prompt.text);
    QLayout *layout = m_promptButtons->layout();
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (int i = 0; i < prompt.buttons.size(); ++i) {
        auto *button = new QPushButton(prompt.buttons.at(i).first);
        button->setAutoDefault(false);
        button->setProperty("accent", i == 0);
        connect(button, &QPushButton::clicked, this, [this, i] { answerPrompt(i); });
        layout->addWidget(button);
    }
    m_promptBar->show();
}

void BrowserWindow::answerPrompt(int button)
{
    auto found = m_prompts.find(currentView());
    if (found == m_prompts.end() || found->isEmpty())
        return;
    const Prompt prompt = found->takeFirst();
    if (button >= 0 && button < prompt.buttons.size()) {
        if (prompt.buttons.at(button).second)
            prompt.buttons.at(button).second();
    } else if (prompt.dismissed) {
        prompt.dismissed();
    }
    showPrompt();
}

void BrowserWindow::offerToSaveLogin(WebView *view, const QString &username, const QString &password)
{
    LoginStore *logins = m_services.logins;
    const QString origin = LoginStore::originOf(view->url());
    if (!logins || m_isPrivate || origin.isEmpty() || !Preferences::instance()->offerToSaveLogins()
        || logins->isNeverSaved(origin) || logins->contains(origin, username, password)) {
        return;
    }
    bool update = false;
    for (const Login &login : logins->loginsFor(origin))
        update = update || login.username == username;

    const QString site = SitePermissions::siteName(QUrl(origin)).toHtmlEscaped();
    const QString who = username.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(username.toHtmlEscaped());
    Prompt prompt;
    prompt.key = QStringLiteral("login:%1:%2").arg(origin, username);
    prompt.icon = Icons::Key;
    if (update) {
        prompt.text = tr("Update the saved password for <b>%1</b>%2?").arg(site, who);
        prompt.buttons = {{tr("Update"), [logins, origin, username, password] {
                               logins->save(origin, username, password);
                           }}};
    } else {
        prompt.text = tr("Save your login for <b>%1</b>%2?").arg(site, who);
        prompt.buttons = {{tr("Save"), [logins, origin, username, password] {
                               logins->save(origin, username, password);
                           }},
                          {tr("Never for this site"), [logins, origin] {
                               logins->setNeverSave(origin, true);
                           }}};
    }
    // The sign-in usually opens a new page; ask on the tab wherever it went.
    dropPrompts(view, prompt.key);
    addPrompt(view, prompt);
}

void BrowserWindow::updateLoginsAction()
{
    if (!m_loginsAction)
        return;
    const WebView *view = currentView();
    const QString origin = view ? LoginStore::originOf(view->url()) : QString();
    m_loginsAction->setVisible(m_services.logins && !origin.isEmpty()
                               && !m_services.logins->loginsFor(origin).isEmpty());
}

void BrowserWindow::showLoginsMenu()
{
    WebView *view = currentView();
    if (!view || !m_services.logins)
        return;
    const QString origin = LoginStore::originOf(view->url());
    QMenu menu(this);
    menu.addAction(tr("Fill in a saved login"))->setEnabled(false);
    menu.addSeparator();
    for (const Login &login : m_services.logins->loginsFor(origin)) {
        QAction *fill = menu.addAction(Icons::icon(Icons::Key),
                                       login.username.isEmpty() ? tr("(no username)") : login.username);
        connect(fill, &QAction::triggered, this, [this, view, login] {
            if (!m_tabBar || indexOfView(view) < 0)
                return;
            // Into the page's sign-in form, from the same isolated world the capture runs in.
            view->page()->runJavaScript(LoginStore::fillScript(login.username, login.password),
                                        QWebEngineScript::ApplicationWorld);
            m_services.logins->markUsed(login.origin, login.username);
        });
    }
    menu.addSeparator();
    connect(menu.addAction(tr("Manage saved logins…")), &QAction::triggered, this,
            &BrowserWindow::showSavedLogins);
    menu.exec(m_omnibox->mapToGlobal(QPoint(m_omnibox->width() - 240, m_omnibox->height())));
}

void BrowserWindow::showSavedLogins()
{
    if (!m_services.logins)
        return;
    LoginsDialog dialog(m_services.logins, this);
    dialog.exec();
}

void BrowserWindow::showSitePermissions()
{
    if (!m_services.profile)
        return;
    SitePermissionsDialog dialog(m_services.profile, this);
    dialog.exec();
}

// ---- Chrome extensions ---------------------------------------------------------------

void BrowserWindow::populateExtensionsMenu()
{
    m_extensionsMenu->clear();
    const QList<ChromeExtensions::Extension> extensions =
        m_services.extensions ? m_services.extensions->extensions() : QList<ChromeExtensions::Extension>();
    bool any = false;
    for (const ChromeExtensions::Extension &extension : extensions) {
        if (!extension.enabled)
            continue;
        any = true;
        QAction *action = m_extensionsMenu->addAction(
            extension.icon.isNull() ? Icons::icon(Icons::Extensions) : extension.icon, extension.name);
        if (extension.popupUrl.isValid()) {
            const QUrl popup = extension.popupUrl;
            connect(action, &QAction::triggered, this, [this, popup] { showExtensionPopup(popup); });
        } else {
            action->setToolTip(tr("This extension works on pages by itself; it has no panel."));
        }
    }
    if (!any)
        m_extensionsMenu->addAction(tr("No extensions yet"))->setEnabled(false);
    m_extensionsMenu->addSeparator();
    connect(m_extensionsMenu->addAction(Icons::icon(Icons::Globe), tr("Chrome Web Store")),
            &QAction::triggered, this, [this] {
                newTab(QUrl(QStringLiteral("https://chromewebstore.google.com/category/extensions")));
            });
    m_extensionsMenu->addAction(m_extensionsAction);
}

void BrowserWindow::showExtensionPopup(const QUrl &url)
{
    // The extension's panel, as Chrome shows it under its toolbar button.
    auto *popup = new QWidget(this, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(1, 1, 1, 1);
    auto *view = new QWebEngineView(m_services.profile, popup);
    layout->addWidget(view);
    // Panels size themselves to their content, within reason.
    connect(view->page(), &QWebEnginePage::contentsSizeChanged, popup, [popup](const QSizeF &size) {
        popup->resize(qBound(240, int(size.width()) + 2, 800), qBound(120, int(size.height()) + 2, 600));
    });
    connect(view->page(), &QWebEnginePage::windowCloseRequested, popup, &QWidget::close);
    view->setUrl(url);
    popup->resize(360, 420);
    const QPoint anchor = m_extensionsButton->mapToGlobal(QPoint(m_extensionsButton->width(),
                                                                 m_extensionsButton->height()));
    popup->move(anchor.x() - popup->width(), anchor.y() + 4);
    popup->show();
}

void BrowserWindow::offerWebStoreExtension(WebView *view)
{
    ChromeExtensions *extensions = m_services.extensions;
    const QString id = ChromeExtensions::webStoreId(view->url());
    // An offer for the extension of a page the tab has left goes with it.
    for (const Prompt &prompt : m_prompts.value(view)) {
        if (prompt.key.startsWith(QStringLiteral("webstore:")) && prompt.key != QStringLiteral("webstore:%1").arg(id))
            dropPrompts(view, prompt.key);
    }
    if (!extensions || !ChromeExtensions::isSupported() || id.isEmpty() || extensions->isInstalled(id))
        return;
    // Only once the store's page is really there: its title is "<name> - Chrome Web Store".
    static const QRegularExpression storeTitle(QStringLiteral("^(.+?)\\s*[-–]\\s*Chrome Web Store\\s*$"));
    const QRegularExpressionMatch match = storeTitle.match(view->title());
    if (!match.hasMatch())
        return;
    const QString name = match.captured(1);
    Prompt prompt;
    prompt.key = QStringLiteral("webstore:%1").arg(id);
    prompt.icon = Icons::Extensions;
    prompt.text = tr("Add <b>%1</b> to SpacePenguin?").arg(name.toHtmlEscaped());
    prompt.buttons = {{tr("Add to SpacePenguin"), [extensions, id, name] {
                           extensions->installFromWebStore(id, name);
                       }}};
    addPrompt(view, prompt);
}

// ---- Tabs on the side, and tab groups ---------------------------------------------

void BrowserWindow::setTabsOnSide(bool side)
{
    m_sideTabsAction->setChecked(side);
    if (side == m_tabsOnSide && m_sideTabs->isHidden() == !side)
        return;
    m_tabsOnSide = side;
    m_tabStrip->setVisible(!side);
    m_sideTabs->setVisible(side);
    // The window buttons go where the title bar is: the tab row, or with the tabs on
    // the side, the toolbar, as in Firefox.
    auto *row = static_cast<QBoxLayout *>((side ? m_navBar : m_tabStrip)->layout());
    row->addWidget(m_frame->controls());
    if (side)
        m_sideTabs->sync();
}

void BrowserWindow::syncSideTabs()
{
    if (m_sideTabs)
        m_sideTabs->sync();
}

void BrowserWindow::refreshGroups()
{
    if (!m_tabBar)
        return;
    m_tabBar->refreshGroups();
    syncSideTabs();
}

void BrowserWindow::addTabToNewGroup(int index)
{
    QObject *tab = tabObject(m_tabBar, index);
    if (!tab)
        return;
    const int group = m_groups->create();
    m_groups->setGroupOf(tab, group);
    m_groups->arrange(m_tabBar);
    refreshGroups();

    // Ask for a name straight away, by the group's chip, as other browsers do.
    QPoint position = QCursor::pos();
    if (!m_tabsOnSide) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_groups->groupOf(tabObject(m_tabBar, i)) == group) {
                position = m_tabBar->mapToGlobal(m_tabBar->tabRect(i).bottomLeft());
                break;
            }
        }
    }
    QTimer::singleShot(0, this, [this, group, position] { showGroupMenu(group, position); });
}

void BrowserWindow::toggleGroupCollapsed(int group)
{
    const bool collapse = !m_groups->isCollapsed(group);
    const int current = m_tabBar->currentIndex();
    if (collapse && m_groups->groupOf(tabObject(m_tabBar, current)) == group) {
        // Step off the group first, to the nearest tab outside it, or a new tab.
        int target = -1;
        for (int distance = 1; distance < m_tabBar->count() && target < 0; ++distance) {
            for (const int candidate : {current + distance, current - distance}) {
                if (candidate < 0 || candidate >= m_tabBar->count())
                    continue;
                const int other = m_groups->groupOf(tabObject(m_tabBar, candidate));
                if (other != group && !m_groups->isCollapsed(other)) {
                    target = candidate;
                    break;
                }
            }
        }
        if (target < 0)
            newTab();
        else
            m_tabBar->setCurrentIndex(target);
    }
    m_groups->setCollapsed(group, collapse);
}

void BrowserWindow::showGroupMenu(int group, const QPoint &globalPosition)
{
    const TabGroup *info = m_groups->group(group);
    if (!info)
        return;

    QMenu menu(this);

    // The name and colour, edited in place at the top of the menu.
    auto *editor = new QWidget(&menu);
    auto *editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(12, 8, 12, 6);
    editorLayout->setSpacing(8);
    auto *name = new QLineEdit(info->title, editor);
    name->setPlaceholderText(tr("Name this group"));
    name->setClearButtonEnabled(true);
    connect(name, &QLineEdit::textChanged, this,
            [this, group](const QString &text) { m_groups->setTitle(group, text.trimmed()); });
    connect(name, &QLineEdit::returnPressed, &menu, &QMenu::close);
    editorLayout->addWidget(name);

    auto *swatches = new QHBoxLayout;
    swatches->setSpacing(6);
    const QStringList colorNames = TabGroups::colorNames();
    const Theme::Colors colors = Theme::currentColors();
    for (int i = 0; i < colorNames.size(); ++i) {
        auto *swatch = new QToolButton(editor);
        swatch->setToolTip(colorNames.at(i));
        swatch->setAccessibleName(colorNames.at(i));
        swatch->setFixedSize(22, 22);
        swatch->setFocusPolicy(Qt::NoFocus);
        const QString ring = i == info->color ? QStringLiteral("2px solid %1").arg(colors.text.name())
                                              : QStringLiteral("none");
        swatch->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: %2; border-radius: 11px; }")
                                  .arg(TabGroups::color(i, colors.dark).name(), ring));
        connect(swatch, &QToolButton::clicked, &menu, [this, group, i, &menu] {
            m_groups->setColor(group, i);
            menu.close();
        });
        swatches->addWidget(swatch);
    }
    swatches->addStretch(1);
    editorLayout->addLayout(swatches);
    auto *editorAction = new QWidgetAction(&menu);
    editorAction->setDefaultWidget(editor);
    menu.addAction(editorAction);
    menu.addSeparator();

    connect(menu.addAction(Icons::icon(Icons::Plus), tr("New Tab in Group")), &QAction::triggered,
            this, [this, group] { newTabInGroup(group); });
    connect(menu.addAction(info->collapsed ? tr("Expand Group") : tr("Collapse Group")),
            &QAction::triggered, this, [this, group] { toggleGroupCollapsed(group); });
    connect(menu.addAction(tr("Ungroup")), &QAction::triggered, this,
            [this, group] { m_groups->ungroup(group); });
    connect(menu.addAction(Icons::icon(Icons::Close), tr("Close Group")), &QAction::triggered,
            this, [this, group] { closeGroup(group); });

    QTimer::singleShot(0, name, [name] {
        name->setFocus();
        name->selectAll();
    });
    menu.exec(globalPosition);
}

void BrowserWindow::newTabInGroup(int group)
{
    WebView *view = newTab();
    // The new tab is last; keeping the group together puts it at the group's end.
    m_groups->setGroupOf(view, group);
    m_groups->arrange(m_tabBar);
    refreshGroups();
    m_tabBar->setCurrentIndex(indexOfView(view));
    m_omnibox->setFocus();
}

void BrowserWindow::closeGroup(int group)
{
    for (int i = m_tabBar->count() - 1; i >= 0; --i) {
        if (m_groups->groupOf(tabObject(m_tabBar, i)) == group)
            closeTab(i);
    }
}

void BrowserWindow::onTabDragFinished(QObject *tab)
{
    const int index = indexOfView(qobject_cast<WebView *>(tab));
    if (index < 0)
        return;

    // Dropped between two tabs of an open group, a tab joins it; dropped next to its
    // own group, it stays; anywhere else, it leaves its group. A collapsed group's
    // chip keeps its group wherever it goes.
    const int own = m_groups->groupOf(tab);
    int group = 0;
    if (own && m_groups->isCollapsed(own)) {
        group = own;
    } else {
        const auto neighbour = [this, index](int step) {
            for (int i = index + step; i >= 0 && i < m_tabBar->count(); i += step) {
                if (m_tabBar->isTabVisible(i)) {
                    const int other = m_groups->groupOf(tabObject(m_tabBar, i));
                    return m_groups->isCollapsed(other) ? 0 : other;
                }
            }
            return 0;
        };
        const int left = neighbour(-1);
        const int right = neighbour(1);
        if (left && left == right)
            group = left;
        else if (own && (own == left || own == right))
            group = own;
    }
    m_groups->setGroupOf(tab, group);
    m_groups->arrange(m_tabBar);
    refreshGroups();
}

// ---- Navigation ---------------------------------------------------------------

void BrowserWindow::loadUrl(WebView *view, const QUrl &url)
{
    if (!view)
        return;
    if (url.scheme().compare(QLatin1String("about"), Qt::CaseInsensitive) == 0
        && url != QUrl(QStringLiteral("about:blank"))) {
        loadAboutPage(view, url.toString());
    } else {
        view->setUrl(url);
    }
    if (view == currentView())
        view->setFocus();
}

void BrowserWindow::goHome()
{
    loadUrl(currentView(), m_resolver.homeUrl());
}

void BrowserWindow::navigate(const QString &text)
{
    WebView *view = currentView();
    if (!view)
        return;

    m_omniboxEdited = false;
    loadUrl(view, m_resolver.resolve(text));
}

AboutPageContext BrowserWindow::aboutContext() const
{
    AboutPageContext context;
    context.appVersion = QCoreApplication::applicationVersion();
    context.isPrivate = m_isPrivate;
    context.platform = QGuiApplication::platformName();
    context.theme = Theme::modeLabel(Theme::storedMode());
    context.buildType = QStringLiteral(SPACEPENGUIN_BUILD_TYPE);
    if (context.buildType.isEmpty())
        context.buildType = QStringLiteral("Default");

    if (m_services.adBlocker) {
        context.filterRules = m_services.adBlocker->ruleCount();
        context.cosmeticRules = m_services.adBlocker->cosmeticRuleCount();
        context.blockingEnabled = m_services.adBlocker->isEnabled();
        context.blockedRequests = m_services.adBlocker->blockedCount();
    }

    if (m_services.userScripts)
        context.extensionCount = m_services.userScripts->scripts().size();

    context.shortcuts.insert(QStringLiteral("history"), Shortcuts::text(Shortcuts::History));
    context.shortcuts.insert(QStringLiteral("downloads"), Shortcuts::text(Shortcuts::Downloads));
    context.shortcuts.insert(QStringLiteral("extensions"), Shortcuts::text(Shortcuts::Extensions));
    context.shortcuts.insert(QStringLiteral("settings"), Shortcuts::text(Shortcuts::Settings));
    context.sandbox = Sandbox::isEnabled()
        ? QStringLiteral("On")
        : QStringLiteral("Off — %1").arg(Sandbox::disabledReason());
    context.device = DeviceTuning::current().device;
    context.deviceAdjustments = DeviceTuning::current().adjustments;

    if (m_services.historyManager && !m_isPrivate) {
        const QLocale locale;
        for (const HistoryEntry &entry : m_services.historyManager->recentHistory(100)) {
            context.history.append(AboutPageEntry{
                entry.title, entry.url.toString(),
                locale.toString(QDateTime::fromSecsSinceEpoch(entry.visitTime),
                                QLocale::ShortFormat)});
        }
    }

    if (m_services.downloadManager) {
        for (const DownloadItem &item : m_services.downloadManager->downloads()) {
            QString status;
            switch (item.state) {
            case DownloadItem::Requested:
            case DownloadItem::InProgress:
                status = QStringLiteral("In progress");
                break;
            case DownloadItem::Completed:
                status = QStringLiteral("Completed");
                break;
            case DownloadItem::Cancelled:
                status = QStringLiteral("Cancelled");
                break;
            case DownloadItem::Interrupted:
                status = QStringLiteral("Failed: %1").arg(item.interruptReason);
                break;
            }
            context.downloads.append(AboutPageEntry{item.suggestedFileName, item.url, status});
        }
    }

    return context;
}

void BrowserWindow::loadAboutPage(WebView *view, const QString &id)
{
    if (!view)
        return;

    const QString normalized = id.trimmed().toLower();
    const AboutPageContext context = aboutContext();
    QString html = AboutPages::isKnown(normalized) ? AboutPages::render(normalized, context)
                                                   : AboutPages::renderUnknown(id);
    if (Theme::storedMode() != Theme::Mode::System) {
        html.replace(QStringLiteral("<html lang=\"en\">"),
                     Theme::isDark() ? QStringLiteral("<html lang=\"en\" class=\"dark\">")
                                     : QStringLiteral("<html lang=\"en\" class=\"light\">"));
    }

    view->page()->setContent(html.toUtf8(), QStringLiteral("text/html;charset=UTF-8"),
                             QUrl(normalized));

    // A few internal pages are doorways to the dialogs that do the real work.
    if (normalized == QLatin1String("about:settings"))
        QTimer::singleShot(0, this, &BrowserWindow::showSettings);
}

void BrowserWindow::onOmniboxReturnPressed()
{
    m_suppressCompleterNavigation = true;
    QTimer::singleShot(0, this, [this] { m_suppressCompleterNavigation = false; });
    navigate(m_omnibox->text());
}

void BrowserWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
        m_fullScreenAction->setChecked(false);
    } else {
        showFullScreen();
        m_fullScreenAction->setChecked(true);
    }
}

void BrowserWindow::setZoom(qreal factor)
{
    WebView *view = currentView();
    if (!view)
        return;

    view->setZoomFactor(qBound(kMinimumZoom, factor, kMaximumZoom));
    updateZoomIndicator();
}

qreal BrowserWindow::currentZoom() const
{
    const WebView *view = currentView();
    return view ? view->zoomFactor() : qreal(1.0);
}

void BrowserWindow::updateZoomIndicator()
{
    const qreal zoom = currentZoom();
    const bool zoomed = qAbs(zoom - 1.0) > 0.001;
    m_zoomButton->setText(formatPercent(zoom));
    m_zoomButton->setVisible(zoomed);
    if (m_zoomMenuLabel)
        m_zoomMenuLabel->setText(formatPercent(zoom));
}

// ---- Find ---------------------------------------------------------------------

void BrowserWindow::showFindBar()
{
    m_findBar->show();
    m_findInput->setFocus(Qt::ShortcutFocusReason);
    m_findInput->selectAll();
    if (!m_findInput->text().isEmpty())
        findNext(false);
}

void BrowserWindow::hideFindBar()
{
    if (m_findView)
        m_findView->page()->findText(QString());
    m_findView = nullptr;
    m_findStatus->clear();
    m_findBar->hide();
    if (WebView *view = currentView())
        view->setFocus();
}

void BrowserWindow::findNext(bool backwards)
{
    WebView *view = currentView();

    // Searching another tab clears the highlight left behind in the previous one.
    if (m_findView && m_findView != view)
        m_findView->page()->findText(QString());
    m_findView = view;

    if (!view || m_findInput->text().isEmpty()) {
        if (view)
            view->page()->findText(QString());
        m_findStatus->clear();
        return;
    }

    QWebEnginePage::FindFlags flags;
    if (backwards)
        flags |= QWebEnginePage::FindBackward;

    QPointer<BrowserWindow> self(this);
    view->page()->findText(m_findInput->text(), flags,
                           [self](const QWebEngineFindTextResult &result) {
                               if (self)
                                   self->findMatchesShown(result.numberOfMatches(),
                                                          result.activeMatch());
                           });
}

void BrowserWindow::findMatchesShown(int matchCount, int activeMatch)
{
    if (m_findInput->text().isEmpty()) {
        m_findStatus->clear();
        return;
    }
    if (matchCount <= 0) {
        m_findStatus->setText(tr("No matches"));
        return;
    }
    m_findStatus->setText(tr("%1 of %2").arg(activeMatch).arg(matchCount));
}

// ---- Completion ---------------------------------------------------------------

void BrowserWindow::refreshCompletions()
{
    if (!m_completionsDirty)
        return;
    m_completionsDirty = false;
    m_completionModel->clear();

    QSet<QString> seen;
    const auto addEntry = [this, &seen](const QUrl &url, const QString &title, bool bookmark) {
        if (!isWebScheme(url))
            return;
        const QString text = url.toString();
        if (seen.contains(text))
            return;
        seen.insert(text);
        auto *item = new QStandardItem;
        item->setData(title.isEmpty() ? url.toDisplayString()
                                      : QStringLiteral("%1  —  %2").arg(title, url.toDisplayString()),
                      Qt::DisplayRole);
        item->setData(text, Qt::UserRole);
        item->setIcon(Icons::icon(bookmark ? Icons::Star : Icons::History));
        m_completionModel->appendRow(item);
    };

    if (m_services.bookmarkManager) {
        for (const Bookmark &bookmark : m_services.bookmarkManager->bookmarks())
            addEntry(bookmark.url, bookmark.title, true);
    }
    if (m_services.historyManager && !m_isPrivate) {
        for (const HistoryEntry &entry : m_services.historyManager->recentHistory(kMaximumCompletions))
            addEntry(entry.url, entry.title, false);
    }
}

// ---- Events -------------------------------------------------------------------

bool BrowserWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_omnibox) {
        // The suggestions popup takes focus and gives it back while you type; that
        // is not the box being focused or left, and must not select what was typed.
        const bool popup = (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)
            && static_cast<QFocusEvent *>(event)->reason() == Qt::PopupFocusReason;
        if (popup) {
            // Nothing to do.
        } else if (event->type() == QEvent::FocusIn) {
            refreshCompletions();
            QTimer::singleShot(0, m_omnibox, &QLineEdit::selectAll);
        } else if (event->type() == QEvent::FocusOut) {
            if (!m_omniboxEdited || m_omnibox->text().isEmpty()) {
                m_omniboxEdited = false;
                if (WebView *view = currentView())
                    syncOmnibox(view->url());
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape && !m_stopAction->isEnabled()) {
                m_omniboxEdited = false;
                if (WebView *view = currentView()) {
                    syncOmnibox(view->url());
                    view->setFocus();
                }
                return true;
            }
        }
    }

    if (watched == m_findInput && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hideFindBar();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            findNext(keyEvent->modifiers() & Qt::ShiftModifier);
            return true;
        }
    }

    if (watched == m_tabBar && event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            const int index = m_tabBar->tabAt(mouseEvent->position().toPoint());
            if (index >= 0) {
                closeTab(index);
                return true;
            }
        }
    }

    if (watched == m_tabStrip && event->type() == QEvent::MouseButtonDblClick) {
        newTab();
        return true;
    }

    if (event->type() == QEvent::Resize && m_progressBar
        && watched == m_progressBar->parentWidget()) {
        auto *content = static_cast<QWidget *>(watched);
        m_progressBar->setGeometry(0, 0, content->width(), 2);
        m_progressBar->raise();
        if (m_linkPreview->isVisible())
            m_linkPreview->move(kLinkPreviewMargin,
                                content->height() - m_linkPreview->height() - kLinkPreviewMargin);
    }

    return QMainWindow::eventFilter(watched, event);
}

void BrowserWindow::onLinkHovered(const QString &url)
{
    if (url.isEmpty()) {
        m_linkPreview->hide();
        return;
    }

    QWidget *content = m_linkPreview->parentWidget();
    const QString text = m_linkPreview->fontMetrics().elidedText(
        QUrl(url).toDisplayString(), Qt::ElideMiddle, qMax(200, content->width() / 2));
    m_linkPreview->setText(text);
    m_linkPreview->adjustSize();
    m_linkPreview->move(kLinkPreviewMargin,
                        content->height() - m_linkPreview->height() - kLinkPreviewMargin);
    m_linkPreview->show();
    m_linkPreview->raise();
}

void BrowserWindow::showToast(const QString &text, int milliseconds)
{
    onLinkHovered(QString());
    m_linkPreview->setText(text);
    m_linkPreview->adjustSize();
    QWidget *content = m_linkPreview->parentWidget();
    m_linkPreview->move(kLinkPreviewMargin,
                        content->height() - m_linkPreview->height() - kLinkPreviewMargin);
    m_linkPreview->show();
    m_linkPreview->raise();

    const QString shown = text;
    QTimer::singleShot(milliseconds, m_linkPreview, [label = m_linkPreview, shown] {
        // A link hovered in the meantime replaced the message; leave that alone.
        if (label->text() == shown)
            label->hide();
    });
}

// ---- View signals -------------------------------------------------------------

void BrowserWindow::connectView(WebView *view)
{
    connect(view, &QWebEngineView::titleChanged, this, [this, view] {
        updateTabLabel(view);
        if (view == currentView())
            updateWindowTitle();
    });
    connect(view, &QWebEngineView::iconChanged, this, [this, view] { updateTabLabel(view); });
    connect(view, &QWebEngineView::urlChanged, this,
            [this, view](const QUrl &url) { onUrlChanged(view, url); });
    connect(view, &QWebEngineView::loadStarted, this, [this, view] { onLoadStarted(view); });
    connect(view, &QWebEngineView::loadProgress, this,
            [this, view](int progress) { onLoadProgress(view, progress); });
    connect(view, &QWebEngineView::loadFinished, this,
            [this, view](bool ok) {
                StartupTrace::markOnce("first page loaded");
                onLoadFinished(view, ok);
            });
    connect(view->page(), &QWebEnginePage::linkHovered, this, [this, view](const QString &url) {
        if (view == currentView())
            onLinkHovered(url);
    });
    connect(view->page(), &QWebEnginePage::windowCloseRequested, this,
            [this, view] { closeTab(indexOfView(view)); });
    connect(view->browserPage(), &BrowserPage::internalPageRequested, this,
            [this, view](const QUrl &url) { loadAboutPage(view, url.toString()); });

    connect(view->page(), &QWebEnginePage::permissionRequested, this,
            [this, view](QWebEnginePermission permission) {
                const QString site = SitePermissions::siteName(permission.origin());
                Prompt prompt;
                prompt.key = QStringLiteral("permission:%1:%2")
                                 .arg(permission.origin().toString())
                                 .arg(int(permission.permissionType()));
                prompt.icon = Icons::Shield;
                prompt.text = tr("<b>%1</b> wants to %2.")
                                  .arg(site.toHtmlEscaped(),
                                       SitePermissions::request(permission.permissionType()));
                prompt.buttons = {{tr("Allow"), [permission] { permission.grant(); }},
                                  {tr("Block"), [permission] { permission.deny(); }}};
                prompt.dismissed = [permission] { permission.deny(); };
                addPrompt(view, prompt);
            });
    // The Chrome Web Store moves between pages without loading them, so its
    // address and title changes count too; the title names the extension.
    connect(view, &QWebEngineView::loadFinished, this, [this, view] { offerWebStoreExtension(view); });
    connect(view, &QWebEngineView::titleChanged, this, [this, view] { offerWebStoreExtension(view); });
    connect(view->browserPage(), &BrowserPage::loginSubmitted, this,
            [this, view](const QString &username, const QString &password) {
                offerToSaveLogin(view, username, password);
            });
    // A question about a page is moot once the tab has gone somewhere else.
    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &url) {
        const QString origin = url.adjusted(QUrl::RemovePath | QUrl::RemoveQuery
                                            | QUrl::RemoveFragment).toString();
        for (const Prompt &prompt : m_prompts.value(view)) {
            if (prompt.key.startsWith(QStringLiteral("permission:"))
                && !prompt.key.startsWith(QStringLiteral("permission:%1:").arg(origin)))
                dropPrompts(view, prompt.key);
        }
    });
    connect(view, &QObject::destroyed, this, [this, view] {
        m_prompts.remove(view);
        showPrompt();
    });

    view->setTabFactory([this](bool background) { return addTab(background); });
    YouTube::attach(view);
}

void BrowserWindow::onCurrentTabChanged(int index)
{
    WebView *view = viewAt(index);
    if (!view)
        return;

    // A tab chosen in a collapsed group (with the keyboard, or on its chip-only
    // tab) opens the group.
    if (const int group = m_groups->groupOf(view); m_groups->isCollapsed(group))
        m_groups->setCollapsed(group, false);

    m_stack->setCurrentWidget(view);
    m_linkPreview->hide();
    m_omniboxEdited = false;
    syncOmnibox(view->url());

    m_progressBar->setValue(view->loadProgress());
    m_progressBar->setVisible(view->isLoading());
    m_reloadStopButton->setDefaultAction(view->isLoading() ? m_stopAction : m_reloadAction);
    m_stopAction->setEnabled(view->isLoading());

    updateNavigationState();
    updateWindowTitle();
    updateSecurityIndicator();
    updateBookmarkStar();
    updateZoomIndicator();
    updateLoginsAction();
    showPrompt();

    if (m_findBar->isVisible())
        findNext(false);

    if (!m_omnibox->hasFocus() || isStartPage(view->url()))
        view->setFocus();
}

void BrowserWindow::onUrlChanged(WebView *view, const QUrl &url)
{
    updateTabLabel(view);
    if (view != currentView())
        return;

    updateLoginsAction();

    syncOmnibox(url);
    updateSecurityIndicator();
    updateNavigationState();
    updateBookmarkStar();
    updateWindowTitle();
}

void BrowserWindow::onLoadStarted(WebView *view)
{
    if (view != currentView())
        return;
    m_stopAction->setEnabled(true);
    m_reloadStopButton->setDefaultAction(m_stopAction);
    m_progressBar->setValue(0);
    m_progressBar->show();
}

void BrowserWindow::onLoadProgress(WebView *view, int progress)
{
    if (view != currentView())
        return;
    m_progressBar->setValue(progress);
    m_progressBar->setVisible(progress < 100);
}

void BrowserWindow::onLoadFinished(WebView *view, bool ok)
{
    if (ok && !m_isPrivate && m_services.historyManager && isWebScheme(view->url()))
        m_services.historyManager->addVisit(view->url(), view->title());

    updateTabLabel(view);
    if (view != currentView())
        return;

    m_progressBar->hide();
    m_stopAction->setEnabled(false);
    m_reloadStopButton->setDefaultAction(m_reloadAction);

    updateSecurityIndicator();
    updateNavigationState();
}

void BrowserWindow::updateNavigationState()
{
    WebView *view = currentView();
    if (!view)
        return;

    m_backAction->setEnabled(view->history()->canGoBack());
    m_forwardAction->setEnabled(view->history()->canGoForward());
}

void BrowserWindow::updateWindowTitle()
{
    WebView *view = currentView();
    QString pageTitle = view ? view->title().trimmed() : QString();
    if (view && isStartPage(view->url()))
        pageTitle.clear();

    QString title = pageTitle.isEmpty() ? QStringLiteral("SpacePenguin")
                                        : pageTitle + QStringLiteral(" — SpacePenguin");
    if (m_isPrivate)
        title += tr(" (Private)");

    setWindowTitle(title);
}

void BrowserWindow::updateSecurityIndicator()
{
    WebView *view = currentView();
    const QUrl url = view ? view->url() : QUrl();
    const QString scheme = url.scheme().toLower();

    if (view && view->lastLoadFailed() && !view->isLoading() && isWebScheme(url)) {
        m_securityAction->setIcon(Icons::icon(Icons::Warning));
        m_securityAction->setToolTip(tr("This page failed to load"));
    } else if (scheme == QLatin1String("https")) {
        m_securityAction->setIcon(Icons::icon(Icons::Lock));
        m_securityAction->setToolTip(tr("Connection to %1 is secure").arg(url.host()));
    } else if (scheme == QLatin1String("http")) {
        m_securityAction->setIcon(Icons::icon(Icons::Warning));
        m_securityAction->setToolTip(tr("Not secure — the connection to %1 is not encrypted")
                                         .arg(url.host()));
    } else if (scheme == QLatin1String("sp")) {
        m_securityAction->setIcon(Icons::icon(Icons::Search));
        m_securityAction->setToolTip(tr("Bundled page, served by SpacePenguin itself"));
    } else if (scheme == QLatin1String("about")) {
        m_securityAction->setIcon(Icons::icon(Icons::Info));
        m_securityAction->setToolTip(tr("Internal page, rendered by SpacePenguin"));
    } else {
        m_securityAction->setIcon(Icons::icon(Icons::Search));
        m_securityAction->setToolTip(tr("Search or enter address"));
    }
}

void BrowserWindow::syncOmnibox(const QUrl &url)
{
    if (m_omnibox->hasFocus() && m_omniboxEdited)
        return;

    // The start page leaves the box empty, ready to type into.
    m_omnibox->setText(isStartPage(url) ? QString() : url.toDisplayString());
    m_omnibox->setCursorPosition(0);
    m_omniboxEdited = false;
}

}

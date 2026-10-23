#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QMainWindow>
#include <QPointer>
#include <QUrl>

#include <functional>

#include "aboutpages.h"
#include "icons.h"
#include "profiles.h"
#include "tabbar.h"
#include "theme.h"
#include "urlresolver.h"
#include "webapps.h"

class QAction;
class QActionGroup;
class QCompleter;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QMenu;
class QProgressBar;
class QShortcut;
class QStackedWidget;
class QStandardItemModel;
class QToolBar;
class QToolButton;
class QWebEngineView;

namespace spacepenguin {

class WindowFrame;

class DownloadsDialog;
class HistoryDialog;
class SideTabs;
class TabGroups;
class WebView;

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrowserWindow(ProfileServices services, QWidget *parent = nullptr);
    ~BrowserWindow() override;

    /** Opens an address, about: page or search in a new tab, as the omnibox would. */
    void openInNewTab(const QString &text);

    /** Opens a new window, private or not, with a tab for each of @p texts. */
    BrowserWindow *openWindow(bool isPrivate, const QStringList &texts = {});

    bool isPrivate() const { return m_isPrivate; }

    /** Opens an installed web app's window (see WebApps). */
    void openApp(const QString &id);
    /** A new tab, for a page coming from outside a browser window (an app's
     *  window): in the window in front, or a new one if there is none. */
    static WebView *tabForOutside(bool background);
    /** The browser window in front, or a new one if none is open. */
    static BrowserWindow *frontWindow();
    /** Makes @p services the normal profile's before any window is open (an app
     *  launched on its own), so a window opened later shares them. */
    static void registerServices(const ProfileServices &services);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct ClosedTab {
        int index = 0;
        QUrl url;
    };

    /** A question for the person about one tab: a site asking for a permission, or
     *  whether to save a login. Shown in the bar above the page while its tab is shown. */
    struct Prompt {
        QString key; // the same question is not asked twice at once
        Icons::Glyph icon = Icons::Info;
        QString text;
        QList<std::pair<QString, std::function<void()>>> buttons; // the first is the main one
        std::function<void()> dismissed;
    };

    // Construction.
    void createActions();
    void createChrome();
    QWidget *createTabStrip();
    QWidget *createNavigationBar();
    void createBookmarksBar();
    QWidget *createFindBar();
    void createMenus();
    QMenu *createAppMenu();
    void createTabShortcuts();

    // Preferences and appearance.
    void applyPreferences();
    void applyTheme(Theme::Mode mode);
    void syncThemeMenu();

    // Made for you (roles.h) and installed apps (webapps.h).
    void applyRoles();
    qreal defaultZoom() const;
    void openDevTools();
    void copyCitation();
    void openWorkPages();
    void installCurrentPageAsApp();
    void finishAppInstall(WebView *view, WebApp app, const QUrl &iconUrl, const QList<QUrl> &pageIcons);
    void confirmAppInstall(const WebApp &app, const QImage &icon);

    // Dialogs.
    void showExtensionsDialog();
    void showDownloads();
    void showHistory();
    void showBookmarks();
    void showSettings();
    void showShortcuts();

    // Blocking.
    void populateBlockingMenu();
    void updateBlockingIndicator();

    // Bookmarks.
    void rebuildBookmarksBar();
    void toggleBookmarkForCurrentPage();
    void updateBookmarkStar();
    QString bookmarkIdForUrl(const QUrl &url) const;

    // Session.
    void restoreSession();
    void saveSession() const;

    // Tabs.
    WebView *currentView() const;
    WebView *viewAt(int index) const;
    int indexOfView(const QWebEngineView *view) const;
    WebView *addTab(bool background);
    WebView *newTab(const QUrl &url = QUrl(), bool background = false);
    BrowserWindow *newWindow(bool isPrivate);
    void closeTab(int index);
    void closeOtherTabs(int index);
    void duplicateTab(int index);
    void reopenClosedTab();
    void selectTabByOffset(int offset);
    void selectTabByIndex(int index);
    void showTabContextMenu(const QPoint &pos);
    void showTabMenu(int index, const QPoint &globalPosition);
    void updateTabLabel(WebView *view);

    // Questions above the page: site permissions and saving logins.
    QWidget *createPromptBar();
    void addPrompt(WebView *view, Prompt prompt);
    void dropPrompts(WebView *view, const QString &keyPrefix);
    void showPrompt();
    void answerPrompt(int button);
    void offerToSaveLogin(WebView *view, const QString &username, const QString &password);
    void updateLoginsAction();
    void showLoginsMenu();
    void showSavedLogins();
    void showSitePermissions();

    // Chrome extensions.
    void populateExtensionsMenu();
    void showExtensionPopup(const QUrl &url);
    void offerWebStoreExtension(WebView *view);
    void offerUserScript(WebView *view, const QUrl &url, bool quiet);
    /** Downloads the @require libraries of a script; @p done gets an error, or nothing. */
    void fetchRequires(const QString &source, std::function<void(const QString &failure)> done);

    // Tabs on the side, and tab groups.
    void setTabsOnSide(bool side);
    void syncSideTabs();
    void refreshGroups();
    void addTabToNewGroup(int index);
    void toggleGroupCollapsed(int group);
    void showGroupMenu(int group, const QPoint &globalPosition);
    void newTabInGroup(int group);
    void closeGroup(int group);
    void onTabDragFinished(QObject *tab);

    // Navigation.
    void navigate(const QString &text);
    void loadUrl(WebView *view, const QUrl &url);
    void loadAboutPage(WebView *view, const QString &id);
    AboutPageContext aboutContext() const;
    void goHome();
    void toggleFullScreen();
    void fillBookmarkMenu(QMenu *menu, const QString &folderId);
    void toggleMuted(int index);
    void toggleReaderView();
    void setPageFullScreen(bool on);
    void exitPageFullScreen();

    void setZoom(qreal factor);
    qreal currentZoom() const;
    void updateZoomIndicator();

    // Find.
    void showFindBar();
    void hideFindBar();
    void findNext(bool backwards);
    void findMatchesShown(int matchCount, int activeMatch);

    // Omnibox completion.
    void refreshCompletions();

    // View signal handlers. Each one acts on the view that sent it.
    void onCurrentTabChanged(int index);
    void onUrlChanged(WebView *view, const QUrl &url);
    void onLoadStarted(WebView *view);
    void onLoadProgress(WebView *view, int progress);
    void onLoadFinished(WebView *view, bool ok);
    void onOmniboxReturnPressed();
    void onLinkHovered(const QString &url);

    void updateNavigationState();
    void updateWindowTitle();
    void updateSecurityIndicator();
    void syncOmnibox(const QUrl &url);
    void connectView(WebView *view);
    void applyDataSaver(WebView *view) const;
    void showToast(const QString &text, int milliseconds);

    ProfileServices m_services;
    bool m_isPrivate = false;
    // A page (a video player) filling the screen.
    bool m_pageFullScreen = false;
    bool m_wasFullScreen = false;
    bool m_wasMaximized = false;
    QPointer<WebView> m_fullScreenView;
    QList<QPointer<QWidget>> m_hiddenForFullScreen;
    QShortcut *m_exitFullScreenShortcut = nullptr;
    UrlResolver m_resolver;

    // Chrome.
    QWidget *m_tabStrip = nullptr;
    WindowFrame *m_frame = nullptr;
    TabBar *m_tabBar = nullptr;
    TabGroups *m_groups = nullptr;
    SideTabs *m_sideTabs = nullptr;
    bool m_tabsOnSide = false;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_navBar = nullptr;
    QToolBar *m_bookmarksBar = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_linkPreview = nullptr;

    QLineEdit *m_omnibox = nullptr;
    QAction *m_securityAction = nullptr;
    QAction *m_bookmarkStarAction = nullptr;
    QAction *m_loginsAction = nullptr;
    QCompleter *m_completer = nullptr;
    QStandardItemModel *m_completionModel = nullptr;
    bool m_completionsDirty = true;
    bool m_omniboxEdited = false;
    bool m_suppressCompleterNavigation = false;

    QToolButton *m_reloadStopButton = nullptr;
    QToolButton *m_homeButton = nullptr;
    QToolButton *m_zoomButton = nullptr;
    QToolButton *m_blockingButton = nullptr;
    QMenu *m_blockingMenu = nullptr;
    QToolButton *m_downloadsButton = nullptr;
    QToolButton *m_extensionsButton = nullptr;
    QMenu *m_extensionsMenu = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QSet<QString> m_offeredScripts; // store pages offer their script once
    QToolButton *m_menuButton = nullptr;
    QLabel *m_zoomMenuLabel = nullptr;

    QWidget *m_findBar = nullptr;
    QLineEdit *m_findInput = nullptr;
    QLabel *m_findStatus = nullptr;
    QPointer<QWebEngineView> m_findView;

    QList<ClosedTab> m_closedTabs;

    QHash<const QObject *, QList<Prompt>> m_prompts;
    QWidget *m_promptBar = nullptr;
    QLabel *m_promptIcon = nullptr;
    QLabel *m_promptText = nullptr;
    QWidget *m_promptButtons = nullptr;

    QPointer<DownloadsDialog> m_downloadsDialog;
    QPointer<HistoryDialog> m_historyDialog;

    // Actions.
    QAction *m_newTabAction = nullptr;
    QAction *m_openFileAction = nullptr;
    QAction *m_muteTabAction = nullptr;
    QAction *m_readerAction = nullptr;
    QAction *m_newWindowAction = nullptr;
    QAction *m_newPrivateWindowAction = nullptr;
    QAction *m_closeTabAction = nullptr;
    QAction *m_closeWindowAction = nullptr;
    QAction *m_reopenTabAction = nullptr;
    QAction *m_nextTabAction = nullptr;
    QAction *m_previousTabAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_reloadAction = nullptr;
    QAction *m_stopAction = nullptr;
    QAction *m_homeAction = nullptr;
    QAction *m_focusAddressAction = nullptr;
    QAction *m_findAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomResetAction = nullptr;
    QAction *m_fullScreenAction = nullptr;
    QAction *m_bookmarkPageAction = nullptr;
    QAction *m_bookmarksBarAction = nullptr;
    QAction *m_menuBarAction = nullptr;
    QAction *m_sideTabsAction = nullptr;
    QAction *m_bookmarksAction = nullptr;
    QAction *m_historyAction = nullptr;
    QAction *m_downloadsAction = nullptr;
    QAction *m_extensionsAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_shortcutsAction = nullptr;
    QAction *m_aboutAction = nullptr;
    QAction *m_installAppAction = nullptr;
    QAction *m_devToolsAction = nullptr;
    QAction *m_viewSourceAction = nullptr;
    QAction *m_focusModeAction = nullptr;
    QAction *m_citationAction = nullptr;
    QAction *m_workPagesAction = nullptr;
    QActionGroup *m_themeGroup = nullptr;
    // Gamer: the tabs this window muted while a page was full screen.
    QList<QPointer<WebView>> m_mutedForFullScreen;
};

}

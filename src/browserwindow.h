#pragma once

#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QTabBar>
#include <QUrl>

#include "aboutpages.h"
#include "profiles.h"
#include "theme.h"
#include "urlresolver.h"

class QAction;
class QActionGroup;
class QCompleter;
class QLabel;
class QLineEdit;
class QMenu;
class QProgressBar;
class QStackedWidget;
class QStandardItemModel;
class QToolBar;
class QToolButton;
class QWebEngineView;

namespace spacepenguin {

class DownloadsDialog;
class HistoryDialog;
class WebView;

/** Tab bar whose tabs shrink to share the available width, like a modern browser. */
class TabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit TabBar(QWidget *parent = nullptr);

protected:
    QSize tabSizeHint(int index) const override;
    QSize minimumTabSizeHint(int index) const override;
};

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrowserWindow(ProfileServices services, QWidget *parent = nullptr);
    ~BrowserWindow() override;

    /** Opens an address, about: page or search in a new tab, as the omnibox would. */
    void openInNewTab(const QString &text);

    bool isPrivate() const { return m_isPrivate; }

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct ClosedTab {
        int index = 0;
        QUrl url;
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
    void newWindow(bool isPrivate);
    void closeTab(int index);
    void closeOtherTabs(int index);
    void duplicateTab(int index);
    void reopenClosedTab();
    void selectTabByOffset(int offset);
    void selectTabByIndex(int index);
    void showTabContextMenu(const QPoint &pos);
    void updateTabLabel(WebView *view);

    // Navigation.
    void navigate(const QString &text);
    void loadUrl(WebView *view, const QUrl &url);
    void loadAboutPage(WebView *view, const QString &id);
    AboutPageContext aboutContext() const;
    void goHome();
    void toggleFullScreen();

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
    UrlResolver m_resolver;

    // Chrome.
    QWidget *m_tabStrip = nullptr;
    TabBar *m_tabBar = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_navBar = nullptr;
    QToolBar *m_bookmarksBar = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_linkPreview = nullptr;

    QLineEdit *m_omnibox = nullptr;
    QAction *m_securityAction = nullptr;
    QAction *m_bookmarkStarAction = nullptr;
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
    QToolButton *m_menuButton = nullptr;
    QLabel *m_zoomMenuLabel = nullptr;

    QWidget *m_findBar = nullptr;
    QLineEdit *m_findInput = nullptr;
    QLabel *m_findStatus = nullptr;
    QPointer<QWebEngineView> m_findView;

    QList<ClosedTab> m_closedTabs;

    QPointer<DownloadsDialog> m_downloadsDialog;
    QPointer<HistoryDialog> m_historyDialog;

    // Actions.
    QAction *m_newTabAction = nullptr;
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
    QAction *m_bookmarksAction = nullptr;
    QAction *m_historyAction = nullptr;
    QAction *m_downloadsAction = nullptr;
    QAction *m_extensionsAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_shortcutsAction = nullptr;
    QAction *m_aboutAction = nullptr;
    QActionGroup *m_themeGroup = nullptr;
};

}

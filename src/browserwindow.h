#pragma once

#include <QList>
#include <QMainWindow>
#include <QUrl>

#include "aboutpages.h"
#include "downloadmanager.h"
#include "profiles.h"
#include "settingsdialog.h"
#include "theme.h"
#include "urlresolver.h"

class QAction;
class QActionGroup;
class QLabel;
class QLineEdit;
class QProgressBar;
class QTabWidget;
class QToolBar;
class QToolButton;
class QWebEngineProfile;
class QWebEngineView;

namespace spacepenguin {

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrowserWindow(ProfileServices services, QWidget *parent = nullptr);
    ~BrowserWindow() override;

    void openInNewTab(const QUrl &url);

    bool isPrivate() const { return m_isPrivate; }

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct ClosedTab {
        int index = 0;
        QUrl url;
    };

    void createActions();
    void createToolBar();
    void createMenus();
    void createTabWidget();
    void createFindBar();
    void createTabShortcuts();
    void createStatusControls();
void applyTheme(Theme::Mode mode);
    void showExtensionsDialog();
    void showDownloads();
    void showSettings();
    void populateBlockingMenu();
    void updateBlockingIndicator();
    void restoreSession();
    void saveSession() const;

    QWebEngineView *currentView() const;
    QWebEngineView *viewAt(int index) const;

    void newTab(const QUrl &url = QUrl());
    void newWindow(bool isPrivate);
    void closeTab(int index);
    void reopenClosedTab();
    void selectTabByOffset(int offset);
    void selectTabByIndex(int index);
    void navigate(const QString &text);
    void loadAboutPage(const QString &id);
    AboutPageContext aboutContext() const;
    void goHome();

    void setZoom(qreal factor);
    qreal currentZoom() const;

    void showFindBar();
    void findNext(bool backwards);
    void findMatchesShown(int matchCount, int activeMatch);

    void onCurrentTabChanged(int index);
    void onTabCloseRequested(int index);
    void onOmniboxReturnPressed();
    void onUrlChanged(const QUrl &url);
    void onTitleChanged(const QString &title);
    void onLoadStarted();
    void onLoadProgress(int progress);
    void onLoadFinished(bool ok);

    void updateNavigationState();
    void updateWindowTitle();
    void updateSecurityIndicator(const QUrl &url);
    void syncOmnibox(const QUrl &url);
    void connectView(QWebEngineView *view);

    ProfileServices m_services;
    bool m_isPrivate = false;
    UrlResolver m_resolver;

    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_omnibox = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_securityLabel = nullptr;
    bool m_omniboxEdited = false;

    QToolBar *m_findToolBar = nullptr;
    QLineEdit *m_findInput = nullptr;
    QLabel *m_findStatus = nullptr;
    QAction *m_findAction = nullptr;

    QList<ClosedTab> m_closedTabs;

    QAction *m_newTabAction = nullptr;
    QAction *m_newWindowAction = nullptr;
    QAction *m_newPrivateWindowAction = nullptr;
    QAction *m_closeTabAction = nullptr;
    QAction *m_closeWindowAction = nullptr;
    QAction *m_reopenTabAction = nullptr;
    QAction *m_nextTabAction = nullptr;
    QAction *m_previousTabAction = nullptr;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_reloadAction = nullptr;
    QAction *m_stopAction = nullptr;
    QAction *m_homeAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomResetAction = nullptr;
    QAction *m_shortcutsAction = nullptr;
    QActionGroup *m_themeGroup = nullptr;
    QToolButton *m_blockingButton = nullptr;
    QMenu *m_blockingMenu = nullptr;
    QAction *m_downloadsAction = nullptr;
};

}
#pragma once

#include <QMainWindow>
#include <QUrl>

#include "urlresolver.h"

class QAction;
class QLabel;
class QLineEdit;
class QProgressBar;
class QTabWidget;
class QToolBar;
class QWebEngineProfile;
class QWebEngineView;

namespace spacepenguin {

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrowserWindow(QWebEngineProfile *profile, QWidget *parent = nullptr);
    ~BrowserWindow() override;

    void openInNewTab(const QUrl &url);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createActions();
    void createToolBar();
    void createMenus();
    void createTabWidget();
    void restoreSession();
    void saveSession() const;
    void syncOmnibox(const QUrl &url);

    QWebEngineView *currentView() const;
    QWebEngineView *viewAt(int index) const;

    void newTab(const QUrl &url = QUrl());
    void closeTab(int index);
    void navigate(const QString &text);
    void goHome();

    void setZoom(qreal factor);
    qreal currentZoom() const;

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
    void connectView(QWebEngineView *view);

    QWebEngineProfile *m_profile = nullptr;
    UrlResolver m_resolver;

    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_omnibox = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_securityLabel = nullptr;
    bool m_omniboxEdited = false;

    QAction *m_newTabAction = nullptr;
    QAction *m_closeTabAction = nullptr;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_reloadAction = nullptr;
    QAction *m_stopAction = nullptr;
    QAction *m_homeAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomResetAction = nullptr;
};

}
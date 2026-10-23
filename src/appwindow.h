#pragma once

#include "webapps.h"

#include <QMainWindow>

#include <functional>

class QWebEngineProfile;

namespace spacepenguin {

class WebView;

/**
 * An installed web app's own window: the site alone, without tabs or an address
 * bar, under its own name and icon. Links to other sites, and pages the site opens
 * in a new tab, go to the browser; the app's own pages stay here.
 *
 * Alt+Left and Alt+Right go back and forward, F5 reloads, Ctrl+Shift+B opens the
 * page in the browser, Ctrl+plus and Ctrl+minus zoom, F11 fills the screen.
 */
class AppWindow : public QMainWindow
{
    Q_OBJECT

public:
    /** Gives a new tab in a browser window, for pages that leave the app. */
    using TabOpener = std::function<WebView *(bool background)>;

    /** Brings the app's window forward, opening one if it has none. Null if the
     *  app is not installed. */
    static AppWindow *open(const QString &id, QWebEngineProfile *profile, TabOpener openTab);

    AppWindow(const WebApp &app, QWebEngineProfile *profile, TabOpener openTab, QWidget *parent = nullptr);

    QString appId() const { return m_app.id; }

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void openInBrowser(const QUrl &url);
    void setZoom(qreal factor);

    WebApp m_app;
    TabOpener m_openTab;
    WebView *m_view = nullptr;
};

}

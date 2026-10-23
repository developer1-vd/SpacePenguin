#pragma once

#include <QElapsedTimer>
#include <QWebEnginePage>

#include <functional>

namespace spacepenguin {

class BrowserPage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit BrowserPage(QWebEngineProfile *profile, QObject *parent = nullptr);

    /** Decides about each page the main frame is about to load; false stops it. An
     *  installed app's window uses it to send other sites to the browser. */
    using NavigationFilter = std::function<bool(const QUrl &url, NavigationType type)>;
    void setNavigationFilter(NavigationFilter filter) { m_navigationFilter = std::move(filter); }

signals:
    /**
     * A link or form on the page pointed at an internal about: page. Chromium cannot
     * render those itself, so the window does it instead.
     */
    void internalPageRequested(const QUrl &url);
    /** A link to a user script (….user.js), as the Install buttons of Greasy Fork and
     *  OpenUserJS are: the window offers to add it instead of showing its code. */
    void userScriptRequested(const QUrl &url);
    /** Someone signed in on the page; see LoginStore::captureScript. */
    void loginSubmitted(const QString &username, const QString &password);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
    /** Page console output stays out of the terminal, as in other browsers, unless
     *  SPACEPENGUIN_JS_CONSOLE=1 asks for it (it is in the developer tools anyway). */
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                  int lineNumber, const QString &sourceID) override;

    static bool isAllowedScheme(const QString &scheme);
    static bool isAboutPage(const QUrl &url);

private:
    NavigationFilter m_navigationFilter;
    QUrl m_hoveredLink;
    QElapsedTimer m_hoverEnded;
};

}

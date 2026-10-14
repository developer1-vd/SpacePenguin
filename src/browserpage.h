#pragma once

#include <QElapsedTimer>
#include <QWebEnginePage>

namespace spacepenguin {

class BrowserPage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit BrowserPage(QWebEngineProfile *profile, QObject *parent = nullptr);

signals:
    /**
     * A link or form on the page pointed at an internal about: page. Chromium cannot
     * render those itself, so the window does it instead.
     */
    void internalPageRequested(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;

    static bool isAllowedScheme(const QString &scheme);
    static bool isAboutPage(const QUrl &url);

private:
    QUrl m_hoveredLink;
    QElapsedTimer m_hoverEnded;
};

}

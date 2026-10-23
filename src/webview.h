#pragma once

#include <QWebEngineView>

#include <functional>

class QContextMenuEvent;
class QWebEngineProfile;

namespace spacepenguin {

class BrowserPage;

/**
 * One tab's view. Tracks load state, which QWebEngineView does not expose, and asks
 * its window for a new tab when the page or its context menu wants one.
 */
class WebView : public QWebEngineView
{
    Q_OBJECT

public:
    using TabFactory = std::function<WebView *(bool background)>;

    explicit WebView(QWebEngineProfile *profile, QWidget *parent = nullptr);

    BrowserPage *browserPage() const { return m_page; }

    void setTabFactory(TabFactory factory) { m_tabFactory = std::move(factory); }

    bool isLoading() const { return m_loading; }
    int loadProgress() const { return m_progress; }
    bool lastLoadFailed() const { return m_failed; }

signals:
    /** Developer mode: Inspect was chosen in the page's context menu. */
    void inspectRequested();

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;
    /** The standard menu; Inspect only for developers, opening the window's developer tools. */
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    WebView *openDialog();

    BrowserPage *m_page = nullptr;
    TabFactory m_tabFactory;
    bool m_loading = false;
    bool m_failed = false;
    int m_progress = 100;
};

}

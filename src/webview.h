#pragma once

#include "mousegestures.h"

#include <QList>
#include <QPoint>
#include <QPointer>
#include <QWebEngineView>

#include <functional>
#include <memory>

class QMouseEvent;

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
    ~WebView() override;

    BrowserPage *browserPage() const { return m_page; }

    void setTabFactory(TabFactory factory) { m_tabFactory = std::move(factory); }

    bool isLoading() const { return m_loading; }
    int loadProgress() const { return m_progress; }
    bool lastLoadFailed() const { return m_failed; }

    /** The page shown in a tab whose page process died, with a Reload link to @p url. */
    static QString crashedPage(const QUrl &url, bool outOfMemory);

signals:
    /** Developer mode: Inspect was chosen in the page's context menu. */
    void inspectRequested();
    /** Picture-in-Picture was chosen in a video's context menu. */
    void pictureInPictureRequested();
    /** Read Aloud was chosen for the selected text. */
    void readAloudRequested();
    /** A mouse gesture was drawn on the page (Preferences::mouseGestures()). */
    void gestureDrawn(spacepenguin::MouseGestures::Action action);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;
    /** The standard menu; Inspect only for developers, opening the window's developer tools. */
    void contextMenuEvent(QContextMenuEvent *event) override;
    /** Watches the widget Chromium draws into, which gets the mouse, for gestures. */
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    WebView *openDialog();

    BrowserPage *m_page = nullptr;
    TabFactory m_tabFactory;
    bool m_loading = false;
    bool m_failed = false;
    int m_progress = 100;

    // A right-button stroke in progress: its points, and the press, held back
    // from the page until it is clear whether this is a gesture or a click.
    QList<QPoint> m_stroke;
    std::unique_ptr<QMouseEvent> m_heldPress;
    QPointer<QObject> m_strokeTarget;
    bool m_replaying = false;
};

}

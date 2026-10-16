#pragma once

#include <QObject>
#include <QPoint>
#include <QPointer>

#include <functional>

class QToolButton;
class QWidget;

namespace spacepenguin {

/**
 * Lets a top-level window draw its own title bar: the system's is turned off and
 * the window's tab strip takes its place, as in Chrome and Firefox.
 *
 * It supplies the minimize, maximize and close buttons, moves the window when the
 * empty part of a drag area is dragged (double-click maximizes), and resizes it
 * from a thin border. Moving and resizing are handed to the window system
 * (QWindow::startSystemMove/startSystemResize), so snapping, tiling and Wayland
 * all behave as they do for system title bars.
 *
 * macOS keeps its own title bar, whose traffic lights people expect there.
 */
class WindowFrame : public QObject
{
    Q_OBJECT

public:
    /** Whether this platform draws its own title bar at all; false on macOS. */
    static bool isSupported();

    /** Width of the resize border, in device-independent pixels. */
    static constexpr int ResizeBorder = 4;
    /** Radius of the window's corners while it is not maximized. */
    static constexpr int CornerRadius = 10;

    /**
     * Call before the window is first shown. Where the corners can only be cut out
     * of a see-through window (Wayland), makes the window see-through.
     */
    static void prepareWindow(QWidget *window);

    explicit WindowFrame(QWidget *window);

    /** The minimize, maximize and close buttons, hidden while the system title bar is used. */
    QWidget *controls();

    /** Makes the empty parts of @p area move the window; @p isEmpty decides what is empty. */
    void addDragArea(QWidget *area, std::function<bool(const QPoint &)> isEmpty = {});

    /** true: the window draws its own title bar; false: the system's. */
    void setCustomTitleBar(bool custom);
    bool hasCustomTitleBar() const { return m_custom; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Qt::Edges edgesAt(const QPoint &position) const;
    void updateFrame();
    void updateCorners();
    void toggleMaximized();

    QWidget *m_window;
    QPointer<QWidget> m_controls;
    QToolButton *m_maximizeButton = nullptr;
    QList<QPair<QWidget *, std::function<bool(const QPoint &)>>> m_dragAreas;
    bool m_custom = false;
    QPoint m_pressPosition;
    bool m_pressed = false;
};

}

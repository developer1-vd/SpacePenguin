#include "windowframe.h"

#include "icons.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStyle>
#include <QToolButton>
#include <QWidget>
#include <QWindow>

namespace spacepenguin {

bool WindowFrame::isSupported()
{
#ifdef Q_OS_MACOS
    return false;
#else
    return true;
#endif
}

WindowFrame::WindowFrame(QWidget *window)
    : QObject(window)
    , m_window(window)
{
    m_window->installEventFilter(this);
    // Hover events over the resize border, to show the resize cursors.
    m_window->setMouseTracking(true);
}

QWidget *WindowFrame::controls()
{
    if (m_controls)
        return m_controls;

    m_controls = new QWidget;
    m_controls->setObjectName(QStringLiteral("windowControls"));
    auto *layout = new QHBoxLayout(m_controls);
    layout->setContentsMargins(6, 0, 0, 0);
    layout->setSpacing(2);

    const auto button = [this, layout](const QString &name, Icons::Glyph glyph,
                                       const QString &toolTip, auto onClick) {
        auto *b = new QToolButton;
        b->setObjectName(name);
        b->setIcon(Icons::icon(glyph));
        b->setIconSize(QSize(16, 16));
        b->setAutoRaise(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(toolTip);
        connect(b, &QToolButton::clicked, this, onClick);
        layout->addWidget(b);
        return b;
    };
    button(QStringLiteral("windowMinimize"), Icons::WindowMinimize, tr("Minimize"),
           [this] { m_window->showMinimized(); });
    m_maximizeButton = button(QStringLiteral("windowMaximize"), Icons::WindowMaximize,
                              tr("Maximize"), [this] { toggleMaximized(); });
    button(QStringLiteral("windowClose"), Icons::Close, tr("Close"), [this] { m_window->close(); });

    m_controls->setVisible(m_custom);
    updateFrame();
    return m_controls;
}

void WindowFrame::addDragArea(QWidget *area, std::function<bool(const QPoint &)> isEmpty)
{
    area->installEventFilter(this);
    m_dragAreas.append({area, std::move(isEmpty)});
}

void WindowFrame::setCustomTitleBar(bool custom)
{
    custom = custom && isSupported();
    if (custom == m_custom && m_window->windowFlags().testFlag(Qt::FramelessWindowHint) == custom)
        return;
    m_custom = custom;

    // Changing window flags recreates the native window, which hides it.
    const bool wasVisible = m_window->isVisible();
    m_window->setWindowFlag(Qt::FramelessWindowHint, custom);
    if (wasVisible)
        m_window->show();

    if (m_controls)
        m_controls->setVisible(custom);
    updateFrame();
}

void WindowFrame::updateFrame()
{
    const bool filled = m_window->isMaximized() || m_window->isFullScreen();
    // The border the window is resized by; none when there is nothing to resize.
    const int border = m_custom && !filled ? ResizeBorder : 0;
    m_window->setContentsMargins(border, border, border, border);
    m_window->setProperty("customFrame", m_custom && !filled);
    m_window->style()->unpolish(m_window);
    m_window->style()->polish(m_window);

    if (m_maximizeButton) {
        m_maximizeButton->setIcon(
            Icons::icon(m_window->isMaximized() ? Icons::WindowRestore : Icons::WindowMaximize));
        m_maximizeButton->setToolTip(m_window->isMaximized() ? tr("Restore") : tr("Maximize"));
    }
}

void WindowFrame::toggleMaximized()
{
    if (m_window->isMaximized())
        m_window->showNormal();
    else
        m_window->showMaximized();
}

Qt::Edges WindowFrame::edgesAt(const QPoint &position) const
{
    if (!m_custom || m_window->isMaximized() || m_window->isFullScreen())
        return {};
    // A little more than the border itself, so the corners are easy to grab.
    const int grip = ResizeBorder + 4;
    Qt::Edges edges;
    if (position.x() < grip)
        edges |= Qt::LeftEdge;
    if (position.x() >= m_window->width() - grip)
        edges |= Qt::RightEdge;
    if (position.y() < grip)
        edges |= Qt::TopEdge;
    if (position.y() >= m_window->height() - grip)
        edges |= Qt::BottomEdge;
    return edges;
}

bool WindowFrame::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        switch (event->type()) {
        case QEvent::WindowStateChange:
            updateFrame();
            break;
        case QEvent::MouseMove: {
            const Qt::Edges edges = edgesAt(static_cast<QMouseEvent *>(event)->position().toPoint());
            if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge))
                m_window->setCursor(Qt::SizeFDiagCursor);
            else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge))
                m_window->setCursor(Qt::SizeBDiagCursor);
            else if (edges & (Qt::LeftEdge | Qt::RightEdge))
                m_window->setCursor(Qt::SizeHorCursor);
            else if (edges & (Qt::TopEdge | Qt::BottomEdge))
                m_window->setCursor(Qt::SizeVerCursor);
            else
                m_window->unsetCursor();
            break;
        }
        case QEvent::Leave:
            m_window->unsetCursor();
            break;
        case QEvent::MouseButtonPress: {
            auto *mouse = static_cast<QMouseEvent *>(event);
            const Qt::Edges edges = edgesAt(mouse->position().toPoint());
            if (mouse->button() == Qt::LeftButton && edges && m_window->windowHandle()) {
                m_window->windowHandle()->startSystemResize(edges);
                return true;
            }
            break;
        }
        default:
            break;
        }
        return false;
    }

    if (!m_custom)
        return false;

    for (const auto &[area, isEmpty] : std::as_const(m_dragAreas)) {
        if (watched != area)
            continue;
        if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseMove
            && event->type() != QEvent::MouseButtonRelease
            && event->type() != QEvent::MouseButtonDblClick) {
            return false;
        }
        auto *mouse = static_cast<QMouseEvent *>(event);
        const QPoint position = mouse->position().toPoint();
        if (isEmpty && !isEmpty(position) && !m_pressed)
            return false;

        switch (event->type()) {
        case QEvent::MouseButtonPress:
            if (mouse->button() == Qt::LeftButton) {
                m_pressed = true;
                m_pressPosition = mouse->globalPosition().toPoint();
                return true;
            }
            break;
        case QEvent::MouseMove:
            // Moving starts once the pointer has really moved, so a double-click
            // still arrives as one.
            if (m_pressed && (mouse->buttons() & Qt::LeftButton)
                && (mouse->globalPosition().toPoint() - m_pressPosition).manhattanLength()
                       >= QApplication::startDragDistance()) {
                m_pressed = false;
                if (m_window->windowHandle())
                    m_window->windowHandle()->startSystemMove();
                return true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if (m_pressed) {
                m_pressed = false;
                return true;
            }
            break;
        case QEvent::MouseButtonDblClick:
            if (mouse->button() == Qt::LeftButton) {
                m_pressed = false;
                toggleMaximized();
                return true;
            }
            break;
        default:
            break;
        }
        return false;
    }
    return false;
}

}

#pragma once

#include <QList>
#include <QPoint>
#include <QString>

namespace spacepenguin {

/**
 * Mouse gestures: hold the right button, draw a stroke on the page, let go.
 *
 * A stroke is read as the directions it moves in, each at least a short distance:
 * "L" for a stroke to the left, "UD" for up then down, and so on. A right click
 * that hardly moves is no gesture and opens the page's menu as usual.
 */
namespace MouseGestures {

enum class Action {
    None,
    Back,
    Forward,
    Reload,
    ScrollToTop,
    ScrollToBottom,
    NewTab,
    CloseTab,
    ReopenClosedTab,
};

/** How far the pointer must move in one direction for it to count, in pixels. */
constexpr int kMinimumStroke = 30;
/** Moving less than this from the press is a plain right click. */
constexpr int kClickSlop = 8;

/** The directions of a stroke through @p points ("L", "UD", …), empty for none. */
QString directions(const QList<QPoint> &points);

/** What a stroke does. */
Action actionFor(const QString &directions);

/** All gestures, for the settings and the help: directions and what they do. */
struct Entry {
    QString directions;
    Action action;
};
QList<Entry> all();

/** "Back", "Close tab"… in the user's language. */
QString label(Action action);
/** "←", "↑↓"… for a stroke's directions. */
QString arrows(const QString &directions);

}

}

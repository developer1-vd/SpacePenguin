#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

namespace spacepenguin {

/**
 * Every keyboard binding in the browser, chosen per platform so that none of them
 * fights the desktop: on macOS Cmd+H hides the app, Cmd+Tab switches apps and F11
 * shows the desktop, so those commands use the bindings Safari and Chrome use there.
 * The window, the start page and the about: pages all read from this one table.
 */
class Shortcuts
{
public:
    enum Command {
        NewTab,
        NewWindow,
        NewPrivateWindow,
        CloseTab,
        CloseWindow,
        ReopenClosedTab,
        NextTab,
        PreviousTab,
        Quit,
        Back,
        Forward,
        Reload,
        Stop,
        Home,
        FocusAddressBar,
        Find,
        ZoomIn,
        ZoomOut,
        ZoomReset,
        FullScreen,
        BookmarkPage,
        BookmarksBar,
        Bookmarks,
        History,
        Downloads,
        Extensions,
        Settings,
        KeyboardShortcuts,
    };

    /** All bindings for a command; the first is the one shown in menus. */
    static QList<QKeySequence> keys(Command command);

    /** The primary binding as the platform writes it ("Ctrl+H", "⌘Y"). */
    static QString text(Command command);

    /** "Ctrl" on Windows and Linux, "⌘" on macOS. */
    static QString primaryModifier();
};

}

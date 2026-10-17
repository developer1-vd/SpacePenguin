#pragma once

#include <QIcon>
#include <QImage>

namespace spacepenguin {

/**
 * The browser's icon set. Every glyph is drawn with QPainter at paint time in the
 * current palette's text colour, so icons stay sharp at any scale, follow the
 * light/dark theme without being regenerated, and look the same on every platform
 * whether or not an icon theme is installed.
 */
class Icons
{
public:
    enum Glyph {
        Back,
        Forward,
        Reload,
        Stop,
        Home,
        Plus,
        Close,
        Menu,
        Star,        // drawn filled with the accent colour in the On state
        Shield,
        Download,
        Lock,
        Warning,
        Info,
        Search,
        ChevronUp,
        ChevronDown,
        History,
        Bookmarks,
        Settings,
        Extensions,
        Globe,
        Private,
        Penguin,     // the app mark, drawn in its own colours
        Folder,
        Open,
        Trash,
        WindowMinimize, // window controls, for the merged title bar
        WindowMaximize,
        WindowRestore,
    };

    enum class AppIconShape {
        Round,  // the disc used in the app, on Windows and on Linux
        MacOS,  // Apple's rounded square, as macOS 26 Tahoe requires
    };

    static QIcon icon(Glyph glyph);
    static QIcon appIcon();

    /** The app mark at @p size pixels, for the icon files (tools/generate-icons.cpp). */
    static QImage appIconImage(int size, AppIconShape shape = AppIconShape::Round);
};

}

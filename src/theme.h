#pragma once

#include <QColor>
#include <QList>
#include <QString>

class QPalette;
class QStyle;

namespace spacepenguin {

class Theme
{
public:
    enum class Mode { System, Light, Dark };

    /**
     * The colours the browser chrome is drawn with. Both the widget palette and the
     * application stylesheet are generated from one of these, so the two never drift.
     */
    struct Colors {
        bool dark = false;
        QColor window;       // dialogs and the navigation bar
        QColor tabStrip;     // behind the tabs
        QColor surface;      // text fields, lists, the active tab
        QColor raised;       // hover and pressed fills
        QColor text;
        QColor mutedText;
        QColor border;
        QColor accent;
        QColor accentText;
        QColor privateAccent;
        QColor danger;
        QColor success;
    };

    static QList<Mode> modes();
    static QString modeKey(Mode mode);
    static QString modeLabel(Mode mode);
    static Mode modeFromKey(const QString &key);

    static Mode storedMode();
    static void storeMode(Mode mode);

    static void apply(Mode mode);
    static Mode effectiveMode();

    /** True when the chrome is currently drawn dark, whatever the stored mode. */
    static bool isDark();
    static Colors colors(bool dark);
    static Colors currentColors();
    static QPalette palette(const Colors &colors);
    static QString styleSheet(const Colors &colors);

    /** Fusion, with the few style hints the chrome stylesheet cannot express. */
    static QStyle *createStyle();
};

}

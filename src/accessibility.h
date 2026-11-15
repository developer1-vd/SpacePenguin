#pragma once

#include <QObject>

class QWidget;

namespace spacepenguin {

/**
 * Names for screen readers. An icon-only button has no text for a screen reader
 * to say, so it would be announced as just "button". Installed on the
 * application, this gives every such button, and every other unnamed control
 * with a tooltip, its tooltip (or its action's text) as its accessible name,
 * and keeps the name in step when the tooltip changes. Names set in the code
 * are left alone.
 */
class AccessibleNames : public QObject
{
    Q_OBJECT

public:
    explicit AccessibleNames(QObject *parent = nullptr);

    /** Installs one on the application. */
    static void install();

    /** The name a screen reader should use for @p widget, if it has none of its own. */
    static QString fallbackName(const QWidget *widget);

    /** Names @p widget if it needs it; returns whether it now has a name. */
    static bool name(QWidget *widget);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

}

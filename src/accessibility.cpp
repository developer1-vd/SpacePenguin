#include "accessibility.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QLineEdit>
#include <QRegularExpression>
#include <QToolButton>
#include <QWidget>

namespace spacepenguin {
namespace {

// Marks a name this class gave, so a later tooltip may replace it; a name set
// anywhere else is the code's own and stays.
constexpr char kGivenName[] = "spacepenguin.givenAccessibleName";

QString plain(QString text)
{
    // "&Back" -> "Back", and a tooltip's shortcut hint stays (it helps too).
    text.remove(QRegularExpression(QStringLiteral("&(?!&)")));
    text.replace(QStringLiteral("&&"), QStringLiteral("&"));
    // Tooltips may be rich text.
    if (text.contains(QLatin1Char('<')))
        text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    return text.simplified();
}

} // namespace

AccessibleNames::AccessibleNames(QObject *parent)
    : QObject(parent)
{
}

void AccessibleNames::install()
{
    static AccessibleNames *names = nullptr;
    if (names)
        return;
    names = new AccessibleNames(qApp);
    qApp->installEventFilter(names);
}

QString AccessibleNames::fallbackName(const QWidget *widget)
{
    // The clear button Qt puts in text fields (setClearButtonEnabled).
    if (widget->inherits("QLineEditIconButton"))
        return QCoreApplication::translate("AccessibleNames", "Clear text");
    if (const auto *button = qobject_cast<const QAbstractButton *>(widget)) {
        // A button with words on it is read out by them already.
        if (!plain(button->text()).isEmpty()) {
            const auto *tool = qobject_cast<const QToolButton *>(button);
            if (!tool || tool->toolButtonStyle() != Qt::ToolButtonIconOnly)
                return {};
        }
        if (const auto *tool = qobject_cast<const QToolButton *>(button); tool && tool->defaultAction()) {
            const QAction *action = tool->defaultAction();
            const QString fromAction = plain(action->toolTip().isEmpty() ? action->text() : action->toolTip());
            if (!fromAction.isEmpty())
                return fromAction;
        }
        if (!plain(button->toolTip()).isEmpty())
            return plain(button->toolTip());
        return plain(button->text());
    }
    // Any other control a person can reach with Tab, named by its tooltip, or a
    // text field by the hint it shows when empty ("Find in page").
    if (widget->focusPolicy() != Qt::NoFocus) {
        if (!plain(widget->toolTip()).isEmpty())
            return plain(widget->toolTip());
        if (const auto *field = qobject_cast<const QLineEdit *>(widget))
            return plain(field->placeholderText());
    }
    return {};
}

bool AccessibleNames::name(QWidget *widget)
{
    const bool ours = widget->property(kGivenName).toBool();
    if (!widget->accessibleName().isEmpty() && !ours)
        return true; // the code named it
    const QString fallback = fallbackName(widget);
    if (fallback.isEmpty())
        return !widget->accessibleName().isEmpty();
    if (widget->accessibleName() != fallback) {
        widget->setAccessibleName(fallback);
        widget->setProperty(kGivenName, true);
    }
    return true;
}

bool AccessibleNames::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Polish:
    case QEvent::ToolTipChange:
    case QEvent::ActionChanged:
    case QEvent::ActionAdded:
        if (watched->isWidgetType())
            name(static_cast<QWidget *>(watched));
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

}

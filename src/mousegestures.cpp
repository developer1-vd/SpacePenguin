#include "mousegestures.h"

#include <QCoreApplication>

#include <cstdlib>

namespace spacepenguin::MouseGestures {

QString directions(const QList<QPoint> &points)
{
    QString result;
    if (points.size() < 2)
        return result;
    // Each direction starts where the last one ended, so a stroke that wobbles a
    // little on its way does not count the wobbles.
    QPoint anchor = points.first();
    for (const QPoint &point : points) {
        const int dx = point.x() - anchor.x();
        const int dy = point.y() - anchor.y();
        if (std::abs(dx) < kMinimumStroke && std::abs(dy) < kMinimumStroke)
            continue;
        QChar direction;
        if (std::abs(dx) >= std::abs(dy))
            direction = dx < 0 ? QLatin1Char('L') : QLatin1Char('R');
        else
            direction = dy < 0 ? QLatin1Char('U') : QLatin1Char('D');
        if (result.isEmpty() || result.back() != direction)
            result += direction;
        anchor = point;
    }
    return result;
}

QList<Entry> all()
{
    return {
        {QStringLiteral("L"), Action::Back},
        {QStringLiteral("R"), Action::Forward},
        {QStringLiteral("U"), Action::ScrollToTop},
        {QStringLiteral("D"), Action::ScrollToBottom},
        {QStringLiteral("UD"), Action::Reload},
        {QStringLiteral("DL"), Action::NewTab},
        {QStringLiteral("DR"), Action::CloseTab},
        {QStringLiteral("UL"), Action::ReopenClosedTab},
    };
}

Action actionFor(const QString &directions)
{
    for (const Entry &entry : all()) {
        if (entry.directions == directions)
            return entry.action;
    }
    return Action::None;
}

QString label(Action action)
{
    switch (action) {
    case Action::None: return QString();
    case Action::Back: return QCoreApplication::translate("MouseGestures", "Back");
    case Action::Forward: return QCoreApplication::translate("MouseGestures", "Forward");
    case Action::Reload: return QCoreApplication::translate("MouseGestures", "Reload");
    case Action::ScrollToTop: return QCoreApplication::translate("MouseGestures", "Scroll to the top");
    case Action::ScrollToBottom: return QCoreApplication::translate("MouseGestures", "Scroll to the bottom");
    case Action::NewTab: return QCoreApplication::translate("MouseGestures", "New tab");
    case Action::CloseTab: return QCoreApplication::translate("MouseGestures", "Close tab");
    case Action::ReopenClosedTab: return QCoreApplication::translate("MouseGestures", "Reopen closed tab");
    }
    return QString();
}

QString arrows(const QString &directions)
{
    QString result;
    for (const QChar direction : directions) {
        switch (direction.unicode()) {
        case 'L': result += QChar(0x2190); break;
        case 'U': result += QChar(0x2191); break;
        case 'R': result += QChar(0x2192); break;
        case 'D': result += QChar(0x2193); break;
        default: break;
        }
    }
    return result;
}

}

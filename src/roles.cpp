#include "roles.h"

#include <QCoreApplication>
#include <QLocale>

namespace spacepenguin::Roles {

QList<Role> all()
{
    return {Role::Developer, Role::Gamer, Role::Student, Role::Worker, Role::Teacher};
}

QString key(Role role)
{
    switch (role) {
    case Role::Developer: return QStringLiteral("developer");
    case Role::Gamer: return QStringLiteral("gamer");
    case Role::Student: return QStringLiteral("student");
    case Role::Worker: return QStringLiteral("worker");
    case Role::Teacher: return QStringLiteral("teacher");
    }
    return QString();
}

QString label(Role role)
{
    switch (role) {
    case Role::Developer: return QCoreApplication::translate("Roles", "Developer");
    case Role::Gamer: return QCoreApplication::translate("Roles", "Gamer");
    case Role::Student: return QCoreApplication::translate("Roles", "Student");
    case Role::Worker: return QCoreApplication::translate("Roles", "Worker");
    case Role::Teacher: return QCoreApplication::translate("Roles", "Teacher");
    }
    return QString();
}

QString description(Role role)
{
    switch (role) {
    case Role::Developer:
        return QCoreApplication::translate(
            "Roles", "Developer tools (F12) and page source (Ctrl+U), browsing without the cache, "
                     "and a user agent of your own.");
    case Role::Gamer:
        return QCoreApplication::translate(
            "Roles", "Games lock the mouse pointer without asking, and other tabs go quiet while "
                     "a game is full screen.");
    case Role::Student:
        return QCoreApplication::translate(
            "Roles", "Join your teacher's class with its code: sites they block show their reason, and "
                     "you can ask to unblock one. Also Focus mode, and Copy Citation for a reference "
                     "to the page.");
    case Role::Worker:
        return QCoreApplication::translate(
            "Roles", "Focus mode, and your work pages opened together, from the menu or at start-up.");
    case Role::Teacher:
        return QCoreApplication::translate(
            "Roles", "Run a class on the school network: block up to 100 sites for the students who "
                     "join, each with your reason, and answer their requests to unblock. Your own "
                     "browsing is not restricted. Also a larger page size for the projector.");
    }
    return QString();
}

bool hasFocusMode(const QStringList &roles)
{
    return roles.contains(key(Role::Student)) || roles.contains(key(Role::Worker));
}

QString normalizeSite(const QString &text)
{
    QString site = text.trimmed().toLower();
    if (site.isEmpty())
        return QString();
    const QUrl url = QUrl::fromUserInput(site);
    if (url.isValid() && !url.host().isEmpty())
        site = url.host();
    if (site.startsWith(QLatin1String("www.")))
        site.remove(0, 4);
    while (site.endsWith(QLatin1Char('.')) || site.endsWith(QLatin1Char('/')))
        site.chop(1);
    return site;
}

bool isBlocked(const QUrl &url, const QStringList &sites)
{
    if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
        return false;
    const QString host = url.host().toLower();
    for (const QString &entry : sites) {
        const QString site = normalizeSite(entry);
        if (site.isEmpty())
            continue;
        if (host == site || host.endsWith(QLatin1Char('.') + site))
            return true;
    }
    return false;
}

QString blockedPage(const QUrl &url)
{
    QString host = url.host();
    if (host.startsWith(QLatin1String("www.")))
        host.remove(0, 4);
    host = host.toHtmlEscaped();
    return QStringLiteral(
               "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
               "<meta name=\"color-scheme\" content=\"light dark\"><title>%1</title><style>"
               "body{font:16px/1.5 system-ui,sans-serif;display:grid;place-items:center;"
               "min-height:90vh;margin:0;text-align:center;color:#3b3f4a}"
               "@media (prefers-color-scheme: dark){body{color:#d6d9e0;background:#15171d}}"
               "h1{font-size:1.6rem;margin:.5rem 0}p{margin:.25rem 0;opacity:.8}"
               ".icon{font-size:3rem}</style></head><body><main>"
               "<div class=\"icon\">🎯</div><h1>%2</h1><p>%3</p><p>%4</p></main></body></html>")
        .arg(QCoreApplication::translate("Roles", "Focus mode").toHtmlEscaped(),
             QCoreApplication::translate("Roles", "%1 can wait").arg(host),
             QCoreApplication::translate("Roles", "Focus mode is keeping this site away for now.")
                 .toHtmlEscaped(),
             QCoreApplication::translate("Roles", "Turn it off from the menu (Focus Mode) when "
                                                  "you are done.")
                 .toHtmlEscaped());
}

QString citation(const QString &title, const QUrl &url, const QDate &accessed)
{
    QString site = url.host();
    if (site.startsWith(QLatin1String("www.")))
        site.remove(0, 4);
    const QString name = title.trimmed().isEmpty() ? site : title.trimmed();
    const QString date = QLocale(QLocale::English).toString(accessed, QStringLiteral("d MMMM yyyy"));
    return QCoreApplication::translate("Roles", "%1. %2. %3 (accessed %4).")
        .arg(name, site, url.toString(QUrl::RemoveUserInfo), date);
}

}

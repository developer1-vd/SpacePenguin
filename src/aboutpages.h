#pragma once

#include <QList>
#include <QString>

namespace spacepenguin {

struct AboutPage {
    QString id;
    QString title;
    QString summary;
    bool easterEgg = false;
};

class AboutPages
{
public:
    static QList<AboutPage> all();
    static AboutPage page(const QString &id);
    static bool isKnown(const QString &id);
    static QStringList ids();

    static QString render(const QString &id, const QString &appVersion, bool isPrivate,
                          const QString &platform = QString());
    static QString renderUnknown(const QString &id);

    static QString htmlShell(const QString &pageTitle, const QString &bodyHtml);
    static QString pageTitle(const QString &id);
};

}
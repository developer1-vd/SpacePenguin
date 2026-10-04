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

struct AboutPageContext {
    QString appVersion;
    bool isPrivate = false;
    QString platform;
    QString theme;
    QString buildType;
    int filterRules = 0;
    int cosmeticRules = 0;
    bool blockingEnabled = false;
    int blockedRequests = 0;
    int extensionCount = 0;
};

class AboutPages
{
public:
    static QList<AboutPage> all();
    static AboutPage page(const QString &id);
    static bool isKnown(const QString &id);
    static QStringList ids();

    static QString render(const QString &id, const AboutPageContext &context);
    static QString renderUnknown(const QString &id);

    static QString htmlShell(const QString &pageTitle, const QString &bodyHtml);
    static QString pageTitle(const QString &id);
};

}
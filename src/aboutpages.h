#pragma once

#include <QList>
#include <QMap>
#include <QString>

namespace spacepenguin {

struct AboutPage {
    QString id;
    QString title;
    QString summary;
    bool easterEgg = false;
};

/** A row on a data-backed internal page such as about:history. */
struct AboutPageEntry {
    QString title;
    QString url;
    QString detail;
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
    /** Renderer sandbox state, e.g. "On" or "Off — <reason>". */
    QString sandbox;
    /** The machine as start-up saw it, and what was adjusted for it. */
    QString device;
    QString deviceAdjustments;
    QList<AboutPageEntry> history;
    QList<AboutPageEntry> downloads;
    /** Shortcut text by command name ("history", "downloads", "extensions",
     *  "settings"), as the platform writes it. Missing entries use Ctrl bindings. */
    QMap<QString, QString> shortcuts;
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
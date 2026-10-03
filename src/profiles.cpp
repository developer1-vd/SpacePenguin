#include "profiles.h"

#include "startpageschemehandler.h"

#include <QDir>
#include <QStandardPaths>
#include <QWebEngineProfile>

namespace spacepenguin {

QString defaultDataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QWebEngineProfile *createProfile(QObject *parent, bool isPrivate)
{
    QWebEngineProfile *profile = nullptr;

    if (isPrivate) {
        profile = new QWebEngineProfile(parent);
    } else {
        const QString dataDirectory = defaultDataDirectory();
        QDir().mkpath(dataDirectory);
        profile = new QWebEngineProfile(QStringLiteral("default"), parent);
        profile->setPersistentStoragePath(dataDirectory);
        profile->setCachePath(dataDirectory + QStringLiteral("/cache"));
    }

    installStartPageHandler(profile);
    return profile;
}

void installStartPageHandler(QWebEngineProfile *profile)
{
    if (!profile)
        return;
    profile->installUrlSchemeHandler(QByteArrayLiteral("sp"),
                                    new StartPageSchemeHandler(profile));
}

}
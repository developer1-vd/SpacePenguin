#pragma once

#include <QString>

class QWebEngineProfile;

namespace spacepenguin {

class AdBlocker;
class UserScripts;

struct ProfileServices {
    QWebEngineProfile *profile = nullptr;
    AdBlocker *adBlocker = nullptr;
    UserScripts *userScripts = nullptr;
};

QString defaultDataDirectory();

ProfileServices createProfileServices(QObject *parent, bool isPrivate,
                                     const QString &filterListPath = QString());

QString extensionsDirectory();
QString defaultFilterListPath();

}
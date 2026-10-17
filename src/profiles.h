#pragma once

#include <QString>

class QWebEngineProfile;

namespace spacepenguin {

class AdBlocker;
class ChromeExtensions;
class BookmarkManager;
class CookieManager;
class DataSaverManager;
class DownloadManager;
class HistoryManager;
class LoginStore;
class UserScripts;

struct ProfileServices {
    QWebEngineProfile *profile = nullptr;
    AdBlocker *adBlocker = nullptr;
    BookmarkManager *bookmarkManager = nullptr;
    CookieManager *cookieManager = nullptr;
    DataSaverManager *dataSaverManager = nullptr;
    DownloadManager *downloadManager = nullptr;
    HistoryManager *historyManager = nullptr;
    LoginStore *logins = nullptr; // shared by every profile
    ChromeExtensions *extensions = nullptr; // none in private windows
    UserScripts *userScripts = nullptr;
};

QString defaultDataDirectory();

ProfileServices createProfileServices(QObject *parent, bool isPrivate,
                                     const QString &filterListPath = QString());

QString extensionsDirectory();
QString defaultFilterListPath();

}
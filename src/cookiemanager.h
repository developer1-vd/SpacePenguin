#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QUrl>

#include <QNetworkCookie>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

namespace spacepenguin {

struct Cookie {
    QString name;
    QString value;
    QUrl domain;
    QString path;
    QDateTime expirationDate;
    bool secure = false;
    bool httpOnly = false;
    bool session = false;
};

class CookieManager : public QObject
{
    Q_OBJECT

public:
    explicit CookieManager(QWebEngineProfile *profile, QObject *parent = nullptr);
    ~CookieManager() override;

    QList<Cookie> allCookies() const;
    QList<Cookie> cookiesForUrl(const QUrl &url) const;

    void deleteCookie(const Cookie &cookie);
    void deleteAllCookies();
    void deleteSessionCookies();
    void deleteCookiesForUrl(const QUrl &url);

signals:
    void cookieAdded(const Cookie &cookie);
    void cookieRemoved(const Cookie &cookie);
    void cookiesCleared();

private:
    QWebEngineProfile *m_profile = nullptr;
    QWebEngineCookieStore *m_cookieStore = nullptr;

    void onCookieAdded(const QNetworkCookie &cookie);
    void onCookieRemoved(const QNetworkCookie &cookie);
    Cookie networkCookieToCookie(const QNetworkCookie &cookie) const;
};

}
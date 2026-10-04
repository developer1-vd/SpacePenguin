#include "cookiemanager.h"

#include <QNetworkCookie>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

namespace spacepenguin {

CookieManager::CookieManager(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent)
    , m_profile(profile)
{
    if (!m_profile) {
        return;
    }

    m_cookieStore = m_profile->cookieStore();
    connect(m_cookieStore, &QWebEngineCookieStore::cookieAdded,
            this, &CookieManager::onCookieAdded);
    connect(m_cookieStore, &QWebEngineCookieStore::cookieRemoved,
            this, &CookieManager::onCookieRemoved);
}

CookieManager::~CookieManager() = default;

QList<Cookie> CookieManager::allCookies() const
{
    QList<Cookie> result;
    if (!m_cookieStore) {
        return result;
    }

    // Note: QWebEngineCookieStore::getAllCookies() is async in Qt6
    // For now we return empty list - in a real implementation you'd use the async API
    return result;
}

QList<Cookie> CookieManager::cookiesForUrl(const QUrl &url) const
{
    QList<Cookie> result;
    if (!m_cookieStore) {
        return result;
    }

    // Note: getCookiesForUrl is async in Qt6
    return result;
}

void CookieManager::deleteCookie(const Cookie &cookie)
{
    if (!m_cookieStore) {
        return;
    }

    QNetworkCookie nc(cookie.name.toUtf8(), cookie.value.toUtf8());
    nc.setDomain(cookie.domain.host());
    nc.setPath(cookie.path);
    nc.setSecure(cookie.secure);
    nc.setHttpOnly(cookie.httpOnly);
    if (!cookie.session) {
        nc.setExpirationDate(cookie.expirationDate);
    }

    m_cookieStore->deleteCookie(nc);
}

void CookieManager::deleteAllCookies()
{
    if (!m_cookieStore) {
        return;
    }

    m_cookieStore->deleteAllCookies();
    emit cookiesCleared();
}

void CookieManager::deleteSessionCookies()
{
    if (!m_cookieStore) {
        return;
    }

    m_cookieStore->deleteSessionCookies();
    emit cookiesCleared();
}

void CookieManager::deleteCookiesForUrl(const QUrl &url)
{
    if (!m_cookieStore) {
        return;
    }

    // Note: This would need async API in Qt6
    // For now, we'll just emit the signal
    emit cookiesCleared();
}

void CookieManager::onCookieAdded(const QNetworkCookie &cookie)
{
    emit cookieAdded(networkCookieToCookie(cookie));
}

void CookieManager::onCookieRemoved(const QNetworkCookie &cookie)
{
    emit cookieRemoved(networkCookieToCookie(cookie));
}

Cookie CookieManager::networkCookieToCookie(const QNetworkCookie &cookie) const
{
    Cookie result;
    result.name = QString::fromUtf8(cookie.name());
    result.value = QString::fromUtf8(cookie.value());
    result.domain = QUrl(QStringLiteral("http://") + cookie.domain());
    result.path = cookie.path();
    result.expirationDate = cookie.expirationDate();
    result.secure = cookie.isSecure();
    result.httpOnly = cookie.isHttpOnly();
    result.session = cookie.isSessionCookie();
    return result;
}

}
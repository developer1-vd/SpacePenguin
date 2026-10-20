#pragma once

#include <QDateTime>
#include <QDialog>
#include <QList>
#include <QObject>
#include <QUrl>

#include <QNetworkCookie>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

class QLineEdit;
class QPushButton;
class QTreeWidget;

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

/**
 * The profile's cookies, kept up to date. Chromium hands them over one by one
 * (QWebEngineCookieStore is asynchronous), so the list is filled at start and
 * follows every cookie set or removed after.
 */
class CookieManager : public QObject
{
    Q_OBJECT

public:
    explicit CookieManager(QWebEngineProfile *profile, QObject *parent = nullptr);
    ~CookieManager() override;

    QList<Cookie> allCookies() const;
    QList<Cookie> cookiesForUrl(const QUrl &url) const;

    /** The site a cookie belongs to, as people know it: "example.com", not ".www.example.com". */
    static QString siteOf(const Cookie &cookie);
    static QString siteOf(const QString &host);

    void deleteCookie(const Cookie &cookie);
    void deleteAllCookies();
    void deleteSessionCookies();
    void deleteCookiesForUrl(const QUrl &url);
    /** Removes every cookie of @p site (as siteOf gives it). */
    void deleteCookiesForSite(const QString &site);

signals:
    void cookieAdded(const Cookie &cookie);
    void cookieRemoved(const Cookie &cookie);
    void cookiesCleared();
    /** The list changed. */
    void changed();

private:
    QWebEngineProfile *m_profile = nullptr;
    QWebEngineCookieStore *m_cookieStore = nullptr;
    QList<QNetworkCookie> m_cookies;

    void onCookieAdded(const QNetworkCookie &cookie);
    void onCookieRemoved(const QNetworkCookie &cookie);
    Cookie networkCookieToCookie(const QNetworkCookie &cookie) const;
};

/** Cookies by site, to look through and remove one site's or one cookie at a time. */
class CookiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CookiesDialog(CookieManager *cookies, QWidget *parent = nullptr);

private:
    void rebuild();

    CookieManager *m_cookies;
    QLineEdit *m_search = nullptr;
    QTreeWidget *m_tree = nullptr;
    QPushButton *m_remove = nullptr;
};

}

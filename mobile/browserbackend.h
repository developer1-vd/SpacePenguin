#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <qqmlintegration.h>

namespace spacepenguin {

/**
 * What the mobile interface needs from C++: turning typed text into addresses,
 * settings shared with the desktop app, bookmarks and history, and the scripts
 * that block YouTube ads and skip sponsor segments.
 *
 * The mobile apps show pages with the phone's own engine (QtWebView: Android
 * WebView, iOS WKWebView), which lets an app run scripts in a page once it has
 * loaded but not block network requests or run anything before the page's own
 * scripts. So ads are hidden and skipped here, not prevented from downloading.
 */
class BrowserBackend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QStringList searchEngines READ searchEngines CONSTANT)
    Q_PROPERTY(int searchEngine READ searchEngine WRITE setSearchEngine NOTIFY settingsChanged)
    Q_PROPERTY(QString searchEngineName READ searchEngineName NOTIFY settingsChanged)
    Q_PROPERTY(bool blockAds READ blockAds WRITE setBlockAds NOTIFY settingsChanged)
    Q_PROPERTY(bool skipSponsors READ skipSponsors WRITE setSkipSponsors NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit BrowserBackend(QObject *parent = nullptr);

    /** An address for what was typed: a URL as it is, anything else a search. */
    Q_INVOKABLE QUrl resolve(const QString &text) const;

    /** Text for the address field: the URL without "https://" clutter. */
    Q_INVOKABLE QString displayUrl(const QUrl &url) const;

    QStringList searchEngines() const;
    int searchEngine() const;
    void setSearchEngine(int index);
    QString searchEngineName() const;

    bool blockAds() const;
    void setBlockAds(bool block);
    bool skipSponsors() const;
    void setSkipSponsors(bool skip);

    QVariantList bookmarks() const;
    Q_INVOKABLE bool isBookmarked(const QUrl &url) const;
    Q_INVOKABLE void toggleBookmark(const QString &title, const QUrl &url);
    Q_INVOKABLE void removeBookmark(const QUrl &url);

    QVariantList history() const;
    Q_INVOKABLE void recordVisit(const QString &title, const QUrl &url);
    Q_INVOKABLE void clearHistory();

    /** The open tabs (not private ones), kept so the app reopens them. */
    Q_INVOKABLE void saveSession(const QVariantMap &session);
    Q_INVOKABLE QVariantMap savedSession() const;

    /**
     * JavaScript to run in a page once it has loaded: for YouTube the ad hiding,
     * ad skipping and sponsor skipping; elsewhere the element-hiding rules of the
     * bundled filter list that apply to the page. Empty when nothing applies.
     */
    Q_INVOKABLE QString pageScript(const QUrl &url) const;

    /**
     * Looks up the sponsor segments of the video @p url plays; sponsorScriptReady
     * then carries the call that hands them to the page.
     */
    Q_INVOKABLE void lookUpSponsors(const QUrl &url);

    QString version() const;

signals:
    void settingsChanged();
    void bookmarksChanged();
    void historyChanged();
    /** Run @p script in the page showing @p url, if it still shows that video. */
    void sponsorScriptReady(const QUrl &url, const QString &script);

private:
    QVariantList readList(const QString &key) const;
    void writeList(const QString &key, const QVariantList &list);

    /** host → CSS selectors, from the "domain##selector" rules of the filter list. */
    QList<QPair<QString, QString>> m_cosmeticRules;
};

}

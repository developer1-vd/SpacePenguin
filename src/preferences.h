#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace spacepenguin {

struct SearchEngine {
    QString name;
    QString searchTemplate; // contains {query}
};

/**
 * User preferences stored in QSettings. Every window and profile listens to
 * changed(), so a setting edited in one place takes effect everywhere at once.
 */
class Preferences : public QObject
{
    Q_OBJECT

public:
    static Preferences *instance();

    /** The search engines to choose from: the built-in ones, then those added. */
    static QList<SearchEngine> searchEngines();
    static QList<SearchEngine> builtInSearchEngines();
    static QList<SearchEngine> addedSearchEngines();
    /** Adds (or renames) a search engine; "%s" in @p searchTemplate counts as {query}.
     *  False if the address is not an http(s) one with a place for the search. */
    bool addSearchEngine(const QString &name, QString searchTemplate);
    void removeSearchEngine(const QString &searchTemplate);
    /** "%s" replaced by {query}; empty if the address cannot be a search engine's. */
    static QString normalizeSearchTemplate(QString searchTemplate);

    QString homePage() const;
    void setHomePage(const QString &url);

    QString searchTemplate() const;
    void setSearchTemplate(const QString &searchTemplate);

    bool restoreSession() const;
    void setRestoreSession(bool restore);

    bool showHomeButton() const;
    void setShowHomeButton(bool show);

    bool showBookmarksBar() const;
    void setShowBookmarksBar(bool show);

    bool showMenuBar() const;
    void setShowMenuBar(bool show);

    /** Show the tabs in a panel down the side of the window instead of in a row on top. */
    bool sideTabs() const;
    void setSideTabs(bool side);

    /** The side panel shows only the tabs' icons. */
    bool sideTabsCollapsed() const;
    void setSideTabsCollapsed(bool collapsed);

    /** Use the system's title bar instead of putting the window buttons in the tab strip. */
    bool useSystemTitleBar() const;
    void setUseSystemTitleBar(bool use);

    QString downloadDirectory() const;
    void setDownloadDirectory(const QString &path);

    bool blockingEnabled() const;
    void setBlockingEnabled(bool enabled);

    /** Offer to save a login when one is used to sign in to a site. */
    bool offerToSaveLogins() const;
    void setOfferToSaveLogins(bool offer);

    /** Skip sponsor segments in YouTube videos, with SponsorBlock's data. */
    bool skipSponsors() const;
    void setSkipSponsors(bool skip);
    /** Whether utm_source, fbclid and the like are taken off addresses. */
    bool stripTracking() const;
    /** Whether to look, once a day, for a newer release (off: nothing is contacted). */
    bool checkForUpdates() const;
    void setCheckForUpdates(bool check);
    void setStripTracking(bool strip);

    // Made for you (see roles.h): what this person uses the browser for, and the
    // settings each role brings.
    QStringList roles() const;
    void setRoles(const QStringList &roles);
    bool hasRole(const QString &role) const;

    /** Developer: load every page fresh, without the HTTP cache. */
    bool bypassCache() const;
    void setBypassCache(bool bypass);
    /** Developer: the user agent sent to sites; empty means the browser's own. */
    QString userAgent() const;
    void setUserAgent(const QString &userAgent);

    /** Gamer: games may hide and lock the mouse pointer without asking. */
    bool allowPointerLock() const;
    void setAllowPointerLock(bool allow);
    /** Gamer: while a page is full screen, the other tabs are muted. */
    bool muteOthersInFullScreen() const;
    void setMuteOthersInFullScreen(bool mute);

    /** Student, worker, teacher: Focus mode is on, and the sites it keeps away. */
    bool focusMode() const;
    void setFocusMode(bool on);
    QStringList focusSites() const;
    void setFocusSites(const QStringList &sites);

    /** Worker: the pages opened by Open Work Pages, and whether they open at start-up. */
    QStringList workPages() const;
    void setWorkPages(const QStringList &pages);
    bool openWorkPagesOnStart() const;
    void setOpenWorkPagesOnStart(bool open);

    /** Teacher: the size new pages open at, in percent (100 is normal). */
    int pageZoom() const;
    void setPageZoom(int percent);

    /** Announces a change made outside the setters, such as the theme. */
    void notifyChanged();

signals:
    void changed();

private:
    explicit Preferences(QObject *parent = nullptr);
    void write(const QString &key, const QVariant &value);
};

}

#pragma once

#include <QList>
#include <QObject>
#include <QString>

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

    static QList<SearchEngine> searchEngines();

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

    /** Announces a change made outside the setters, such as the theme. */
    void notifyChanged();

signals:
    void changed();

private:
    explicit Preferences(QObject *parent = nullptr);
    void write(const QString &key, const QVariant &value);
};

}

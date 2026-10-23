#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

namespace spacepenguin {

/** One installed web app: a site that opens in a window of its own. */
struct WebApp {
    QString id;
    QString name;
    QUrl startUrl;
    QUrl scope;      // pages under it stay in the app's window; the rest open in the browser
    QString themeColor;

    bool isValid() const { return !id.isEmpty() && startUrl.isValid(); }
    /** Whether @p url belongs to the app (same origin, under its scope's path). */
    bool contains(const QUrl &url) const;
};

/**
 * Progressive web apps, installed on this computer: Install App in the menu reads
 * the site's web app manifest (its name, icon, start page and scope) and adds a
 * launcher where the system keeps them: the app menu on Linux, the Start menu on
 * Windows, ~/Applications on macOS. The launcher runs SpacePenguin --app=<id>,
 * which opens the site in a window of its own, without tabs or an address bar.
 *
 * The list lives in the settings; each app's icon in the data folder.
 */
class WebApps : public QObject
{
    Q_OBJECT

public:
    static WebApps *instance();

    QList<WebApp> apps() const;
    WebApp app(const QString &id) const;
    QImage icon(const QString &id) const;

    /** Saves the app and its icon and adds its launcher. On failure, @p error says why. */
    bool install(WebApp app, const QImage &icon, QString *error = nullptr);
    /** Removes the app, its icon and its launcher. */
    void remove(const QString &id);

    /** A stable id from the start page: "github-com-3fa1b2". */
    static QString idFor(const QUrl &startUrl);

    /**
     * The app a web app manifest describes, @p manifestUrl being where it was read
     * from (relative addresses are resolved against it) and @p pageUrl the page
     * that named it. @p iconUrl gets the best icon: PNG or similar, the largest up
     * to 512 pixels. Exposed for testing.
     */
    static WebApp fromManifest(const QByteArray &json, const QUrl &manifestUrl, const QUrl &pageUrl,
                               QUrl *iconUrl = nullptr);

    /** The program and arguments that start this SpacePenguin again (an AppImage,
     *  an unpacked AppImage, a Flatpak or a plain binary). */
    static QStringList launcherCommand();
    /** The Linux launcher for @p app (a .desktop file). Exposed for testing. */
    static QString desktopEntry(const WebApp &app, const QString &program, const QString &iconPath);

signals:
    void changed();

private:
    explicit WebApps(QObject *parent = nullptr);

    QString iconPath(const QString &id) const;
    bool addLauncher(const WebApp &app, const QImage &icon, QString *error);
    void removeLauncher(const WebApp &app);
};

}

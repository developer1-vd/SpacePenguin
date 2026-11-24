#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace spacepenguin {

/**
 * Links that belong to apps on the computer rather than to the web: "Open in app"
 * buttons (discord://, zoommtg:, spotify:), games (roblox-player:), sign-in pages
 * sending you back to the app that asked (vscode://…?code=…), mailto: and tel:.
 * Chromium leaves them to the browser; SpacePenguin asks first, then hands them to
 * whatever the system opens them with (xdg-open, the desktop portal in the
 * Flatpak, Launch Services, the Windows shell).
 *
 * And Flatpak apps: a .flatpakref downloaded from Flathub's Install button can be
 * installed from the browser.
 */
namespace ExternalApps {

/** Whether @p url is for an app on the computer: a scheme that is not the web's
 *  or the browser's own, and not one that is never opened (see isBlocked). */
bool isAppLink(const QUrl &url);
/** Link types that are never opened: they run programs, open local files or
 *  Windows' troubleshooters and search on the page's say (javascript:, file:,
 *  ms-msdt:, search-ms:, …), or are the browser's own. */
bool isBlocked(const QString &scheme);

/** What the person is shown for @p scheme: the app the system opens it with
 *  when that can be found (Linux), a known name (Roblox, Zoom), or the scheme. */
QString appName(const QString &scheme);
/** Whether something on the computer opens @p scheme. True when that cannot be
 *  told (macOS, Windows, inside the Flatpak), so only a definite no stops a link. */
bool hasHandler(const QString &scheme);
/** Roblox's links (roblox:, roblox-player:, roblox-studio:). */
bool isRoblox(const QString &scheme);

/** The site a link came from, as "Always open" remembers it: its host. */
QString siteOf(const QUrl &page);
bool isAlwaysAllowed(const QString &site, const QString &scheme);
void setAlwaysAllowed(const QString &site, const QString &scheme);

/** The link as the page wrote it. Qt refuses some that apps use (Discord's
 *  "discord://-/…": a "-" is no host name), keeping only the original text in
 *  its error message; it is read back from there. Empty if it cannot be. */
QString linkText(const QUrl &url);
/** Hands @p url to the system. False if nothing could be started. */
bool open(const QUrl &url);

/** A Flatpak app reference, as Flathub's Install button downloads it. */
struct FlatpakRef {
    QString name;  // org.example.App
    QString title; // its name for people, if the file has one
    QString branch;
    QUrl repository;
    bool isValid() const { return !name.isEmpty() && repository.isValid(); }
};
FlatpakRef parseFlatpakRef(const QByteArray &contents);
/** Whether @p path is a Flatpak reference (.flatpakref) by its name. */
bool isFlatpakRef(const QString &path);
/** Whether SpacePenguin can install it itself, with the flatpak command: it is
 *  there and SpacePenguin is not itself a Flatpak. Otherwise the file goes to the
 *  system's software centre. */
bool canInstallFlatpaks();
/** Sober runs Roblox on Linux, from Flathub. */
QUrl soberPage();

}

}

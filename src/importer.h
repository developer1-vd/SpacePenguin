#pragma once

#include "logins.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace spacepenguin {

/**
 * Bringing bookmarks and logins over from other browsers.
 *
 * Bookmarks: Chrome, Edge, Brave and Chromium keep theirs in a JSON file that is
 * read directly; every browser, Firefox and Safari included, can also export a
 * bookmarks HTML file. Logins: browsers encrypt their saved passwords, so they
 * come from the CSV file Chrome, Edge, Firefox and password managers export.
 */
namespace Importer {

struct ImportedBookmark {
    QStringList folders; // the folder path within the import, outermost first
    QString title;
    QUrl url;
};

struct Browser {
    QString name;
    QString bookmarksFile;
};

/** The Chromium-based browsers found on this computer, with their bookmarks file. */
QList<Browser> installedBrowsers();

QList<ImportedBookmark> parseChromiumBookmarks(const QByteArray &json);
/** The Netscape bookmarks format every browser exports. */
QList<ImportedBookmark> parseBookmarksHtml(const QString &html);
/** Logins from an exported CSV file; rows without an address or password are left out. */
QList<Login> parseLoginsCsv(const QString &csv);

}

}

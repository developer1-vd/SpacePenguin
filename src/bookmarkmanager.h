#pragma once

#include "importer.h"

#include <QList>
#include <QPair>
#include <QObject>
#include <QString>
#include <QUrl>

namespace spacepenguin {

struct Bookmark {
    QString id;
    QString title;
    QUrl url;
    QString folderId;
    qint64 createdAt = 0;
    int visitCount = 0;
    qint64 lastVisited = 0;
};

class BookmarkManager : public QObject
{
    Q_OBJECT

public:
    explicit BookmarkManager(QObject *parent = nullptr);
    ~BookmarkManager() override;

    QList<Bookmark> bookmarks() const { return m_bookmarks; }
    QList<Bookmark> rootBookmarks() const;
    QList<Bookmark> folderBookmarks(const QString &folderId) const;

    bool addBookmark(const Bookmark &bookmark);
    bool removeBookmark(const QString &id);
    bool updateBookmark(const Bookmark &bookmark);

    QString createFolder(const QString &title, const QString &parentFolderId = QString());

    /** A folder is a bookmark without an address. */
    static bool isFolder(const Bookmark &bookmark) { return bookmark.url.isEmpty(); }
    /** What a folder holds (the top level for an empty id), oldest first, as shown. */
    QList<Bookmark> children(const QString &folderId) const;
    /** Every folder, with its path ("Work › Projects"), for choosing one. */
    QList<QPair<QString, QString>> folderPaths() const;

    /**
     * Adds bookmarks brought over from another browser in one go, inside a new folder
     * named @p folderTitle, keeping their folders and order. Addresses already
     * bookmarked are skipped. Returns how many were added.
     */
    int importBookmarks(const QList<Importer::ImportedBookmark> &bookmarks, const QString &folderTitle);

    void loadFromSettings();
    void saveToSettings();

signals:
    void bookmarkAdded(const Bookmark &bookmark);
    void bookmarkRemoved(const QString &id);
    void bookmarkUpdated(const Bookmark &bookmark);

private:
    QList<Bookmark> m_bookmarks;
};

}
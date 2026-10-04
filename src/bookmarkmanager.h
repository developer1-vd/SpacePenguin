#pragma once

#include <QList>
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

    void loadFromSettings();
    void saveToSettings();

signals:
    void bookmarkAdded(const Bookmark &bookmark);
    void bookmarkRemoved(const QString &id);
    void bookmarkUpdated(const Bookmark &bookmark);

private:
    QList<Bookmark> m_bookmarks;
    QString m_rootFolderId;
};

}
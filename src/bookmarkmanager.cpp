#include "bookmarkmanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>
#include <QHash>
#include <QSet>

#include <functional>

namespace spacepenguin {

BookmarkManager::BookmarkManager(QObject *parent)
    : QObject(parent)
{
    loadFromSettings();
}

BookmarkManager::~BookmarkManager() = default;

QList<Bookmark> BookmarkManager::rootBookmarks() const
{
    QList<Bookmark> result;
    for (const Bookmark &bookmark : m_bookmarks) {
        if (bookmark.folderId.isEmpty()) {
            result.append(bookmark);
        }
    }
    return result;
}

QList<Bookmark> BookmarkManager::folderBookmarks(const QString &folderId) const
{
    QList<Bookmark> result;
    for (const Bookmark &bookmark : m_bookmarks) {
        if (bookmark.folderId == folderId) {
            result.append(bookmark);
        }
    }
    return result;
}

bool BookmarkManager::addBookmark(const Bookmark &bookmark)
{
    Bookmark newBookmark = bookmark;
    if (newBookmark.id.isEmpty()) {
        newBookmark.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (newBookmark.createdAt == 0) {
        newBookmark.createdAt = QDateTime::currentSecsSinceEpoch();
    }

    m_bookmarks.prepend(newBookmark);
    saveToSettings();
    emit bookmarkAdded(newBookmark);
    return true;
}

bool BookmarkManager::removeBookmark(const QString &id)
{
    // A folder goes with everything in it.
    QSet<QString> doomed{id};
    for (bool grew = true; grew;) {
        grew = false;
        for (const Bookmark &bookmark : std::as_const(m_bookmarks)) {
            if (doomed.contains(bookmark.folderId) && !doomed.contains(bookmark.id)) {
                doomed.insert(bookmark.id);
                grew = true;
            }
        }
    }
    const qsizetype before = m_bookmarks.size();
    m_bookmarks.removeIf([&doomed](const Bookmark &bookmark) { return doomed.contains(bookmark.id); });
    if (m_bookmarks.size() == before)
        return false;
    saveToSettings();
    emit bookmarkRemoved(id);
    return true;
}

QList<Bookmark> BookmarkManager::children(const QString &folderId) const
{
    // Kept newest first; shown oldest first, as in every other browser.
    QList<Bookmark> result;
    for (const Bookmark &bookmark : m_bookmarks) {
        if (bookmark.folderId == folderId)
            result.prepend(bookmark);
    }
    return result;
}

QList<QPair<QString, QString>> BookmarkManager::folderPaths() const
{
    QList<QPair<QString, QString>> paths;
    const std::function<void(const QString &, const QString &)> walk =
        [&](const QString &parent, const QString &prefix) {
            for (const Bookmark &bookmark : children(parent)) {
                if (!isFolder(bookmark))
                    continue;
                const QString path = prefix.isEmpty() ? bookmark.title : prefix + QStringLiteral(" › ") + bookmark.title;
                paths.append({bookmark.id, path});
                walk(bookmark.id, path);
            }
        };
    walk(QString(), QString());
    return paths;
}

int BookmarkManager::importBookmarks(const QList<Importer::ImportedBookmark> &bookmarks,
                                     const QString &folderTitle)
{
    QSet<QString> existing;
    for (const Bookmark &bookmark : std::as_const(m_bookmarks))
        existing.insert(bookmark.url.toString());

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QList<Bookmark> added; // in the order they are shown
    QHash<QString, QString> folderIds; // folder path → id
    const auto folderFor = [&](const QStringList &path) {
        QString parent = folderIds.value(QString());
        for (int depth = 1; depth <= path.size(); ++depth) {
            const QString key = path.mid(0, depth).join(QChar(0x1f));
            if (!folderIds.contains(key)) {
                Bookmark folder;
                folder.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                folder.title = path.at(depth - 1);
                folder.folderId = parent;
                folder.createdAt = now;
                added.append(folder);
                folderIds.insert(key, folder.id);
            }
            parent = folderIds.value(key);
        }
        return parent;
    };
    Bookmark top;
    top.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    top.title = folderTitle;
    top.createdAt = now;
    added.append(top);
    folderIds.insert(QString(), top.id);

    int count = 0;
    for (const Importer::ImportedBookmark &imported : bookmarks) {
        if (existing.contains(imported.url.toString()))
            continue;
        existing.insert(imported.url.toString());
        Bookmark bookmark;
        bookmark.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        bookmark.title = imported.title.isEmpty() ? imported.url.host() : imported.title;
        bookmark.url = imported.url;
        bookmark.folderId = folderFor(imported.folders);
        bookmark.createdAt = now;
        added.append(bookmark);
        ++count;
    }
    if (count == 0)
        return 0;
    // Stored newest first: what is shown first goes last.
    for (const Bookmark &bookmark : std::as_const(added))
        m_bookmarks.prepend(bookmark);
    saveToSettings();
    emit bookmarkAdded(top);
    return count;
}

bool BookmarkManager::updateBookmark(const Bookmark &bookmark)
{
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].id == bookmark.id) {
            m_bookmarks[i] = bookmark;
            saveToSettings();
            emit bookmarkUpdated(bookmark);
            return true;
        }
    }
    return false;
}

QString BookmarkManager::createFolder(const QString &title, const QString &parentFolderId)
{
    Bookmark folder;
    folder.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    folder.title = title;
    folder.folderId = parentFolderId;
    folder.createdAt = QDateTime::currentSecsSinceEpoch();

    m_bookmarks.prepend(folder);
    saveToSettings();
    emit bookmarkAdded(folder);
    return folder.id;
}

void BookmarkManager::loadFromSettings()
{
    QSettings settings;
    const QByteArray data = settings.value("bookmarks/data").toByteArray();
    if (data.isEmpty())
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return;
    }

    const QJsonArray array = doc.array();
    m_bookmarks.clear();
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        Bookmark bookmark;
        bookmark.id = obj.value("id").toString();
        bookmark.title = obj.value("title").toString();
        bookmark.url = QUrl(obj.value("url").toString());
        bookmark.folderId = obj.value("folderId").toString();
        bookmark.createdAt = obj.value("createdAt").toVariant().toLongLong();
        bookmark.visitCount = obj.value("visitCount").toInt();
        bookmark.lastVisited = obj.value("lastVisited").toVariant().toLongLong();
        m_bookmarks.append(bookmark);
    }

    // Older versions made an empty "Bookmarks Bar" folder nothing used; it would now
    // show up as a folder on the bar.
    m_bookmarks.removeIf([this](const Bookmark &bookmark) {
        return isFolder(bookmark) && bookmark.title == QLatin1String("Bookmarks Bar")
            && bookmark.folderId.isEmpty() && folderBookmarks(bookmark.id).isEmpty();
    });
}

void BookmarkManager::saveToSettings()
{
    QSettings settings;
    QJsonArray array;
    for (const Bookmark &bookmark : m_bookmarks) {
        QJsonObject obj;
        obj["id"] = bookmark.id;
        obj["title"] = bookmark.title;
        obj["url"] = bookmark.url.toString();
        obj["folderId"] = bookmark.folderId;
        obj["createdAt"] = bookmark.createdAt;
        obj["visitCount"] = bookmark.visitCount;
        obj["lastVisited"] = bookmark.lastVisited;
        array.append(obj);
    }

    const QJsonDocument doc(array);
    settings.setValue("bookmarks/data", doc.toJson(QJsonDocument::Compact));
}

}
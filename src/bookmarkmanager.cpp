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

namespace spacepenguin {

BookmarkManager::BookmarkManager(QObject *parent)
    : QObject(parent)
{
    m_rootFolderId = QUuid::createUuid().toString(QUuid::WithoutBraces);
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
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].id == id) {
            m_bookmarks.removeAt(i);
            saveToSettings();
            emit bookmarkRemoved(id);
            return true;
        }
    }
    return false;
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
    if (data.isEmpty()) {
        // Create default bookmarks
        Bookmark folder;
        folder.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        folder.title = QStringLiteral("Bookmarks Bar");
        folder.folderId = QString();
        folder.createdAt = QDateTime::currentSecsSinceEpoch();
        m_bookmarks.append(folder);
        m_rootFolderId = folder.id;
        return;
    }

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

    if (m_rootFolderId.isEmpty() && !m_bookmarks.isEmpty()) {
        m_rootFolderId = m_bookmarks.first().id;
    }
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
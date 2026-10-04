#include "historymanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>

namespace spacepenguin {

HistoryManager::HistoryManager(QObject *parent)
    : QObject(parent)
{
    loadFromSettings();
}

HistoryManager::~HistoryManager() = default;

QList<HistoryEntry> HistoryManager::history(int limit, const QString &search) const
{
    QList<HistoryEntry> result;
    for (const HistoryEntry &entry : m_history) {
        if (search.isEmpty() ||
            entry.url.toString().contains(search, Qt::CaseInsensitive) ||
            entry.title.contains(search, Qt::CaseInsensitive)) {
            result.append(entry);
        }
    }

    if (limit > 0 && result.size() > limit) {
        result = result.mid(0, limit);
    }
    return result;
}

QList<HistoryEntry> HistoryManager::recentHistory(int limit) const
{
    QList<HistoryEntry> result = m_history;
    std::sort(result.begin(), result.end(),
              [](const HistoryEntry &a, const HistoryEntry &b) {
                  return a.visitTime > b.visitTime;
              });
    if (result.size() > limit) {
        result = result.mid(0, limit);
    }
    return result;
}

QList<HistoryEntry> HistoryManager::mostVisited(int limit) const
{
    QList<HistoryEntry> result = m_history;
    std::sort(result.begin(), result.end(),
              [](const HistoryEntry &a, const HistoryEntry &b) {
                  return a.visitCount > b.visitCount;
              });
    if (result.size() > limit) {
        result = result.mid(0, limit);
    }
    return result;
}

void HistoryManager::addVisit(const QUrl &url, const QString &title, const QUrl &referrer)
{
    if (!url.isValid() || url.scheme().isEmpty()) {
        return;
    }

    // Check if URL already exists
    for (auto &entry : m_history) {
        if (entry.url == url) {
            entry.visitCount++;
            entry.visitTime = QDateTime::currentSecsSinceEpoch();
            entry.title = title;
            if (!referrer.isEmpty()) {
                entry.referrer = referrer.toString();
            }
            saveToSettings();
            emit visitAdded(entry);
            return;
        }
    }

    // New entry
    HistoryEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.url = url;
    entry.title = title.isEmpty() ? url.toString() : title;
    entry.visitTime = QDateTime::currentSecsSinceEpoch();
    entry.visitCount = 1;
    entry.referrer = referrer.isValid() ? referrer.toString() : QString();

    m_history.prepend(entry);
    saveToSettings();
    emit visitAdded(entry);
}

void HistoryManager::removeEntry(const QString &id)
{
    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history[i].id == id) {
            m_history.removeAt(i);
            saveToSettings();
            emit visitRemoved(id);
            return;
        }
    }
}

void HistoryManager::clearHistory()
{
    m_history.clear();
    saveToSettings();
    emit historyCleared();
}

void HistoryManager::clearHistoryForUrl(const QUrl &url)
{
    for (int i = m_history.size() - 1; i >= 0; --i) {
        if (m_history[i].url == url) {
            m_history.removeAt(i);
        }
    }
    saveToSettings();
    emit historyCleared();
}

void HistoryManager::clearHistoryForPeriod(const QDateTime &since, const QDateTime &until)
{
    for (int i = m_history.size() - 1; i >= 0; --i) {
        const qint64 time = m_history[i].visitTime;
        if (time >= since.toSecsSinceEpoch() && time <= until.toSecsSinceEpoch()) {
            m_history.removeAt(i);
        }
    }
    saveToSettings();
    emit historyCleared();
}

int HistoryManager::totalVisits() const
{
    int total = 0;
    for (const auto &entry : m_history) {
        total += entry.visitCount;
    }
    return total;
}

int HistoryManager::uniqueUrls() const
{
    return m_history.size();
}

void HistoryManager::loadFromSettings()
{
    QSettings settings;
    const QByteArray data = settings.value("history/entries").toByteArray();
    if (data.isEmpty()) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return;
    }

    const QJsonArray array = doc.array();
    m_history.clear();
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        HistoryEntry entry;
        entry.id = obj.value("id").toString();
        entry.url = QUrl(obj.value("url").toString());
        entry.title = obj.value("title").toString();
        entry.visitTime = obj.value("visitTime").toVariant().toLongLong();
        entry.visitCount = obj.value("visitCount").toInt();
        entry.referrer = obj.value("referrer").toString();
        entry.isBookmarked = obj.value("isBookmarked").toBool();
        m_history.append(entry);
    }
}

void HistoryManager::saveToSettings()
{
    QSettings settings;
    QJsonArray array;
    for (const HistoryEntry &entry : m_history) {
        QJsonObject obj;
        obj["id"] = entry.id;
        obj["url"] = entry.url.toString();
        obj["title"] = entry.title;
        obj["visitTime"] = entry.visitTime;
        obj["visitCount"] = entry.visitCount;
        obj["referrer"] = entry.referrer;
        obj["isBookmarked"] = entry.isBookmarked;
        array.append(obj);
    }

    const QJsonDocument doc(array);
    settings.setValue("history/entries", doc.toJson(QJsonDocument::Compact));
}

}
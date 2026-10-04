#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QDateTime>

namespace spacepenguin {

struct HistoryEntry {
    QString id;
    QUrl url;
    QString title;
    qint64 visitTime = 0;
    int visitCount = 0;
    QString referrer;
    bool isBookmarked = false;
};

class HistoryManager : public QObject
{
    Q_OBJECT

public:
    explicit HistoryManager(QObject *parent = nullptr);
    ~HistoryManager() override;

    QList<HistoryEntry> history(int limit = -1, const QString &search = QString()) const;
    QList<HistoryEntry> recentHistory(int limit = 50) const;
    QList<HistoryEntry> mostVisited(int limit = 10) const;

    void addVisit(const QUrl &url, const QString &title, const QUrl &referrer = QUrl());
    void removeEntry(const QString &id);
    void clearHistory();
    void clearHistoryForUrl(const QUrl &url);
    void clearHistoryForPeriod(const QDateTime &since, const QDateTime &until);

    int totalVisits() const;
    int uniqueUrls() const;

signals:
    void visitAdded(const HistoryEntry &entry);
    void visitRemoved(const QString &id);
    void historyCleared();

private:
    void loadFromSettings();
    void saveToSettings();

    QList<HistoryEntry> m_history;
    QString m_settingsKey = "history/entries";
};

}
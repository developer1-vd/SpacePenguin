#pragma once

#include "adblocker.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <memory>

class QNetworkAccessManager;
class QTimer;

namespace spacepenguin {

/**
 * The filter lists the blockers use: the person's own list (filters.txt) plus the
 * well-known lists they subscribe to (EasyList and EasyPrivacy by default), which
 * are downloaded into the profile and brought up to date every few days.
 *
 * The lists are parsed off the window's thread and handed to every blocker
 * attached, so private windows block the same way as normal ones.
 */
class FilterLists : public QObject
{
    Q_OBJECT

public:
    struct List {
        QString id;
        QString name;
        QString description;
        QUrl url;
        bool onByDefault = false;
    };

    static FilterLists *instance();

    /** The lists one can subscribe to. */
    static QList<List> catalog();

    /** Where downloaded lists are kept. Set once at startup, with the person's own list. */
    void setUserList(const QString &path, bool subscriptions);
    QString userListPath() const { return m_userListPath; }

    bool isSubscribed(const QString &id) const;
    void setSubscribed(const QString &id, bool subscribed);
    /** When a list was last downloaded; invalid if never. */
    QDateTime updated(const QString &id) const;
    QString listFile(const QString &id) const;
    bool isUpdating() const { return m_pending > 0; }

    /** Keeps @p blocker loaded with the current lists. */
    void attach(AdBlocker *blocker);

    /** Reads the lists again, as after the person edits their own. */
    void reload();
    /** Downloads the subscribed lists: all of them, or only those over a few days old. */
    void update(bool all);

signals:
    /** The lists in use, or their download state, changed. */
    void changed();
    /** A download failed; @p message says which and why. */
    void updateFailed(const QString &message);

private:
    explicit FilterLists(QObject *parent = nullptr);
    void apply(std::shared_ptr<const AdBlocker::Filters> filters, int generation);

    QString m_userListPath;
    bool m_subscriptions = true;
    QList<QPointer<AdBlocker>> m_blockers;
    std::shared_ptr<const AdBlocker::Filters> m_current;
    int m_generation = 0;
    int m_pending = 0;
    QNetworkAccessManager *m_network = nullptr;
    QTimer *m_timer = nullptr;
};

}

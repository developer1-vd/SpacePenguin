#include "filterlists.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QSettings>
#include <QThread>
#include <QTimer>

namespace spacepenguin {
namespace {

const char *const kSubscriptionsKey = "blocking/subscriptions";
// Lists change daily, but a few days old still blocks almost everything.
constexpr qint64 kStaleSeconds = 4 * 24 * 60 * 60;
constexpr int kCheckIntervalMs = 6 * 60 * 60 * 1000;
constexpr int kFirstCheckDelayMs = 3000;

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

/** Whether a download looks like a filter list, not an error page. */
bool looksLikeFilterList(const QByteArray &data)
{
    int rules = 0;
    for (const QByteArray &line : data.split('\n')) {
        if (line.startsWith("||") || line.contains("##") || line.startsWith("@@"))
            ++rules;
        if (rules >= 10)
            return true;
    }
    return false;
}

} // namespace

FilterLists *FilterLists::instance()
{
    static FilterLists *lists = new FilterLists(qApp);
    return lists;
}

FilterLists::FilterLists(QObject *parent)
    : QObject(parent)
{
}

QList<FilterLists::List> FilterLists::catalog()
{
    return {
        {QStringLiteral("easylist"), tr("EasyList"), tr("The main list of ads, used by most blockers."),
         QUrl(QStringLiteral("https://easylist.to/easylist/easylist.txt")), true},
        {QStringLiteral("easyprivacy"), tr("EasyPrivacy"), tr("Trackers and analytics."),
         QUrl(QStringLiteral("https://easylist.to/easylist/easyprivacy.txt")), true},
        {QStringLiteral("peterlowe"), tr("Peter Lowe's list"), tr("Ad and tracking servers; small and quick."),
         QUrl(QStringLiteral("https://pgl.yoyo.org/adservers/serverlist.php?hostformat=adblockplus&showintro=0&mimetype=plaintext")),
         false},
        {QStringLiteral("ublock"), tr("uBlock filters"), tr("uBlock Origin's own fixes for sites the other lists miss."),
         QUrl(QStringLiteral("https://ublockorigin.github.io/uAssets/filters/filters.txt")), false},
        {QStringLiteral("annoyances"), tr("Fanboy's Annoyances"), tr("Cookie notices, pop-ups and social widgets."),
         QUrl(QStringLiteral("https://secure.fanboy.co.nz/fanboy-annoyance.txt")), false},
    };
}

void FilterLists::setUserList(const QString &path, bool subscriptions)
{
    m_userListPath = path;
    m_subscriptions = subscriptions;
    reload();
    if (!m_subscriptions || m_timer)
        return;
    // Out-of-date lists are fetched soon after startup, then checked a few times a day.
    m_timer = new QTimer(this);
    m_timer->setInterval(kCheckIntervalMs);
    connect(m_timer, &QTimer::timeout, this, [this] { update(false); });
    m_timer->start();
    QTimer::singleShot(kFirstCheckDelayMs, this, [this] { update(false); });
}

bool FilterLists::isSubscribed(const QString &id) const
{
    QSettings settings;
    if (settings.contains(QLatin1String(kSubscriptionsKey)))
        return settings.value(QLatin1String(kSubscriptionsKey)).toStringList().contains(id);
    for (const List &list : catalog()) {
        if (list.id == id)
            return list.onByDefault;
    }
    return false;
}

void FilterLists::setSubscribed(const QString &id, bool subscribed)
{
    if (isSubscribed(id) == subscribed)
        return;
    QStringList ids;
    for (const List &list : catalog()) {
        if (list.id == id ? subscribed : isSubscribed(list.id))
            ids.append(list.id);
    }
    QSettings().setValue(QLatin1String(kSubscriptionsKey), ids);
    // A new list is fetched now; one dropped stops blocking now.
    if (subscribed && !QFile::exists(listFile(id)))
        update(false);
    reload();
}

QString FilterLists::listFile(const QString &id) const
{
    const QString folder = m_userListPath.isEmpty() ? QString() : QFileInfo(m_userListPath).absolutePath();
    return QDir(folder).filePath(QStringLiteral("lists/%1.txt").arg(id));
}

QDateTime FilterLists::updated(const QString &id) const
{
    const QFileInfo info(listFile(id));
    return info.exists() ? info.lastModified() : QDateTime();
}

void FilterLists::attach(AdBlocker *blocker)
{
    m_blockers.append(blocker);
    blocker->setListPath(m_userListPath);
    if (m_current) {
        blocker->setFilters(m_current);
    } else if (!m_userListPath.isEmpty()) {
        // The person's own list now, so blocking starts at once; the big lists follow.
        blocker->loadFilters(m_userListPath);
    }
}

void FilterLists::reload()
{
    QStringList files;
    if (!m_userListPath.isEmpty())
        files.append(m_userListPath);
    if (m_subscriptions) {
        for (const List &list : catalog()) {
            if (isSubscribed(list.id) && QFile::exists(listFile(list.id)))
                files.append(listFile(list.id));
        }
    }
    const int generation = ++m_generation;
    QThread *thread = QThread::create([this, files, generation] {
        QString text;
        for (const QString &file : files)
            text += readText(file) + QLatin1Char('\n');
        std::shared_ptr<const AdBlocker::Filters> filters = AdBlocker::parseFilters(text);
        QMetaObject::invokeMethod(this, [this, filters, generation] { apply(filters, generation); },
                                  Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start(QThread::LowPriority);
}

void FilterLists::apply(std::shared_ptr<const AdBlocker::Filters> filters, int generation)
{
    if (generation != m_generation)
        return; // a newer reload is on its way
    m_current = std::move(filters);
    for (const QPointer<AdBlocker> &blocker : std::as_const(m_blockers)) {
        if (blocker)
            blocker->setFilters(m_current);
    }
    emit changed();
}

void FilterLists::update(bool all)
{
    if (!m_subscriptions || m_userListPath.isEmpty())
        return;
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    const QDateTime now = QDateTime::currentDateTime();
    for (const List &list : catalog()) {
        if (!isSubscribed(list.id))
            continue;
        const QDateTime when = updated(list.id);
        if (!all && when.isValid() && when.secsTo(now) < kStaleSeconds)
            continue;
        QNetworkRequest request(list.url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SpacePenguin"));
        request.setTransferTimeout(60000);
        QNetworkReply *reply = m_network->get(request);
        ++m_pending;
        emit changed();
        const QString id = list.id;
        const QString name = list.name;
        connect(reply, &QNetworkReply::finished, this, [this, reply, id, name] {
            reply->deleteLater();
            const QByteArray data = reply->readAll();
            if (reply->error() != QNetworkReply::NoError) {
                emit updateFailed(tr("%1 could not be downloaded: %2").arg(name, reply->errorString()));
            } else if (!looksLikeFilterList(data)) {
                emit updateFailed(tr("%1 did not download as a filter list.").arg(name));
            } else {
                QDir().mkpath(QFileInfo(listFile(id)).absolutePath());
                QSaveFile file(listFile(id));
                if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit())
                    reload();
                else
                    emit updateFailed(tr("%1 could not be saved: %2").arg(name, file.errorString()));
            }
            --m_pending;
            emit changed();
        });
    }
}

}

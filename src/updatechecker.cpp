#include "updatechecker.h"

#include "preferences.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QVersionNumber>

namespace spacepenguin {
namespace {

const char *const kLastCheckKey = "updates/lastCheck";
const char *const kLatestUrl = "https://api.github.com/repos/developer1-vd/SpacePenguin/releases/latest";
constexpr qint64 kCheckIntervalSeconds = 24 * 60 * 60;

} // namespace

UpdateChecker *UpdateChecker::instance()
{
    static UpdateChecker *checker = new UpdateChecker(qApp);
    return checker;
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
{
}

bool UpdateChecker::isNewer(const QString &latest, const QString &current)
{
    QString tag = latest.trimmed();
    if (tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        tag.remove(0, 1);
    const QVersionNumber newest = QVersionNumber::fromString(tag);
    return !newest.isNull() && newest > QVersionNumber::fromString(current);
}

void UpdateChecker::checkIfDue()
{
    if (!Preferences::instance()->checkForUpdates())
        return;
    QSettings settings;
    const QDateTime last = settings.value(QLatin1String(kLastCheckKey)).toDateTime();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (last.isValid() && last.secsTo(now) < kCheckIntervalSeconds)
        return;
    settings.setValue(QLatin1String(kLastCheckKey), now);

    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    QNetworkRequest request{QUrl(QString::fromLatin1(kLatestUrl))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SpacePenguin"));
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return; // no release yet, or offline: try again tomorrow
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        if (isNewer(tag, QCoreApplication::applicationVersion()))
            emit updateAvailable(tag, QUrl(release.value(QStringLiteral("html_url")).toString()));
    });
}

}

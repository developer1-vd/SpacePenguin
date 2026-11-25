#include "authenticatorpage.h"

#include "authenticator.h"
#include "theme.h"

#include <QBuffer>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWebEngineUrlRequestJob>

namespace spacepenguin::AuthenticatorPage {
namespace {

void reply(QWebEngineUrlRequestJob *job, const QByteArray &type, const QByteArray &data)
{
    auto *buffer = new QBuffer(job);
    buffer->setData(data);
    buffer->open(QIODevice::ReadOnly);
    job->reply(type, buffer);
}

void replyJson(QWebEngineUrlRequestJob *job, const QJsonObject &object)
{
    reply(job, QByteArrayLiteral("application/json"), QJsonDocument(object).toJson(QJsonDocument::Compact));
}

void replyError(QWebEngineUrlRequestJob *job, const QString &message)
{
    replyJson(job, QJsonObject{{QStringLiteral("error"), message}});
}

QJsonObject body(QWebEngineUrlRequestJob *job)
{
    QIODevice *device = job->requestBody();
    if (!device)
        return {};
    if (!device->isOpen())
        device->open(QIODevice::ReadOnly);
    return QJsonDocument::fromJson(device->read(64 * 1024)).object();
}

} // namespace

bool isAllowedCaller(const QUrl &initiator)
{
    return initiator.scheme() == QLatin1String("sp") && initiator.host() == QLatin1String("authenticator");
}

void handle(QWebEngineUrlRequestJob *job, AuthenticatorStore *store, const QByteArray &pageHtml)
{
    const QString path = job->requestUrl().path();

    // The page itself holds no keys and no codes, only the code that asks for them.
    if (path.isEmpty() || path == QLatin1String("/")) {
        QByteArray page = pageHtml;
        if (Theme::hasKnownScheme()) {
            page.replace(QByteArrayLiteral("<html lang=\"en\">"),
                         Theme::isDark() ? QByteArrayLiteral("<html lang=\"en\" class=\"dark\">")
                                         : QByteArrayLiteral("<html lang=\"en\" class=\"light\">"));
        }
        page.replace(QByteArrayLiteral("/*SP_ACCENT*/"), Theme::accentCss());
        reply(job, QByteArrayLiteral("text/html; charset=utf-8"), page);
        return;
    }

    if (!store || !isAllowedCaller(job->initiator())) {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    if (path == QLatin1String("/api/list")) {
        QJsonArray list;
        for (const OtpAccount &account : store->accounts()) {
            const bool totp = account.type == OtpAccount::Type::Totp;
            list.append(QJsonObject{
                {QStringLiteral("id"), account.id},
                {QStringLiteral("issuer"), account.issuer},
                {QStringLiteral("name"), account.name},
                {QStringLiteral("site"), QUrl(account.site).host()},
                {QStringLiteral("type"), totp ? QStringLiteral("totp") : QStringLiteral("hotp")},
                {QStringLiteral("digits"), account.digits},
                {QStringLiteral("period"), account.period},
                // An HOTP code is used up once shown, so it comes from /api/next only.
                {QStringLiteral("code"), totp ? Otp::code(account, now) : QString()},
                {QStringLiteral("left"), totp ? Otp::secondsLeft(account, now) : 0},
            });
        }
        replyJson(job, QJsonObject{{QStringLiteral("now"), now}, {QStringLiteral("accounts"), list}});
        return;
    }
    if (path == QLatin1String("/api/info")) {
        replyJson(job, QJsonObject{{QStringLiteral("storage"), store->storageDescription()}});
        return;
    }
    if (job->requestMethod() != "POST") {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }

    const QJsonObject request = body(job);
    const QString id = request.value(QStringLiteral("id")).toString();
    if (path == QLatin1String("/api/add")) {
        QString error;
        const QString uri = request.value(QStringLiteral("uri")).toString().trimmed();
        OtpAccount account = !uri.isEmpty()
            ? Otp::parseUri(uri, &error)
            : Otp::fromKey(request.value(QStringLiteral("issuer")).toString(),
                           request.value(QStringLiteral("name")).toString(),
                           request.value(QStringLiteral("key")).toString(), &error);
        if (!account.isValid()) {
            replyError(job, error);
            return;
        }
        if (account.issuer.isEmpty() && account.name.isEmpty()) {
            replyError(job, QObject::tr("Give the account a name, such as the site it's for."));
            return;
        }
        replyJson(job, QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("id"), store->add(account)}});
        return;
    }
    if (store->account(id).id.isEmpty()) {
        replyError(job, QObject::tr("That account is no longer in the authenticator."));
        return;
    }
    if (path == QLatin1String("/api/next")) {
        replyJson(job, QJsonObject{{QStringLiteral("code"), store->takeCode(id, now)}});
        return;
    }
    if (path == QLatin1String("/api/rename")) {
        const QString issuer = request.value(QStringLiteral("issuer")).toString();
        const QString name = request.value(QStringLiteral("name")).toString();
        if (issuer.trimmed().isEmpty() && name.trimmed().isEmpty()) {
            replyError(job, QObject::tr("Give the account a name, such as the site it's for."));
            return;
        }
        store->rename(id, issuer, name);
        replyJson(job, QJsonObject{{QStringLiteral("ok"), true}});
        return;
    }
    if (path == QLatin1String("/api/remove")) {
        store->remove(id);
        replyJson(job, QJsonObject{{QStringLiteral("ok"), true}});
        return;
    }
    if (path == QLatin1String("/api/export")) {
        const OtpAccount account = store->account(id);
        replyJson(job, QJsonObject{{QStringLiteral("uri"), Otp::toUri(account)},
                                   {QStringLiteral("key"), Otp::base32Encode(account.secret)}});
        return;
    }
    job->fail(QWebEngineUrlRequestJob::UrlNotFound);
}

}

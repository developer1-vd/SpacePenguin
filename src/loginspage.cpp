#include "loginspage.h"

#include "logins.h"
#include "theme.h"

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWebEngineUrlRequestJob>

namespace spacepenguin::LoginsPage {
namespace {

void reply(QWebEngineUrlRequestJob *job, const QByteArray &type, const QByteArray &data)
{
    auto *buffer = new QBuffer(job);
    buffer->setData(data);
    buffer->open(QIODevice::ReadOnly);
    job->reply(type, buffer);
}

void replyJson(QWebEngineUrlRequestJob *job, const QJsonValue &value)
{
    const QByteArray data = value.isArray() ? QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact)
                                            : QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
    reply(job, QByteArrayLiteral("application/json"), data);
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
    return initiator.scheme() == QLatin1String("sp") && initiator.host() == QLatin1String("logins");
}

void handle(QWebEngineUrlRequestJob *job, LoginStore *logins, const QByteArray &pageHtml)
{
    const QString path = job->requestUrl().path();

    // The page itself: it holds no logins, only the code that asks for them.
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

    if (!logins || !isAllowedCaller(job->initiator())) {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }
    const bool post = job->requestMethod() == "POST";

    if (path == QLatin1String("/api/list")) {
        QJsonArray list;
        for (const Login &login : logins->logins()) {
            list.append(QJsonObject{
                {QStringLiteral("origin"), login.origin},
                {QStringLiteral("site"), QUrl(login.origin).host()},
                {QStringLiteral("username"), login.username},
                {QStringLiteral("password"), login.password},
                {QStringLiteral("lastUsed"), login.lastUsed.isValid() ? login.lastUsed.toString(Qt::ISODate) : QString()},
            });
        }
        replyJson(job, list);
        return;
    }
    if (path == QLatin1String("/api/never")) {
        replyJson(job, QJsonArray::fromStringList(logins->neverSaved()));
        return;
    }
    if (!post) {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }

    const QJsonObject request = body(job);
    if (path == QLatin1String("/api/save")) {
        QString site = request.value(QStringLiteral("site")).toString().trimmed();
        // A bare "example.com" means the secure site, where people sign in.
        if (!site.isEmpty() && !site.contains(QLatin1String("://")))
            site.prepend(QLatin1String("https://"));
        const QString origin = LoginStore::originOf(QUrl::fromUserInput(site));
        const QString username = request.value(QStringLiteral("username")).toString();
        const QString password = request.value(QStringLiteral("password")).toString();
        if (origin.isEmpty() || password.isEmpty()) {
            replyJson(job, QJsonObject{{QStringLiteral("error"),
                                        QStringLiteral("A login needs a website address and a password.")}});
            return;
        }
        // Editing a login can change its site or name: the old one goes.
        const QString oldOrigin = request.value(QStringLiteral("oldOrigin")).toString();
        const QString oldUsername = request.value(QStringLiteral("oldUsername")).toString();
        if (!oldOrigin.isEmpty() && (oldOrigin != origin || oldUsername != username))
            logins->remove(oldOrigin, oldUsername);
        logins->save(origin, username, password);
        replyJson(job, QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("origin"), origin}});
        return;
    }
    if (path == QLatin1String("/api/remove")) {
        logins->remove(request.value(QStringLiteral("origin")).toString(),
                       request.value(QStringLiteral("username")).toString());
        replyJson(job, QJsonObject{{QStringLiteral("ok"), true}});
        return;
    }
    if (path == QLatin1String("/api/allow")) {
        logins->setNeverSave(request.value(QStringLiteral("origin")).toString(), false);
        replyJson(job, QJsonObject{{QStringLiteral("ok"), true}});
        return;
    }
    job->fail(QWebEngineUrlRequestJob::UrlNotFound);
}

}

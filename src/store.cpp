#include "store.h"

#include "preferences.h"
#include "theme.h"
#include "userextensions.h"

#include <QBuffer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QUrlQuery>
#include <QWebEngineUrlRequestJob>

#include <algorithm>
#include <memory>

namespace spacepenguin {
namespace {

constexpr int kSearchTimeoutMs = 15000;
const char *const kGreasyFork = "Greasy Fork";
const char *const kOpenUserJs = "OpenUserJS";

/** The writing a text is in, roughly: what tells a Chinese script from an English one. */
QString writingOf(const QString &text)
{
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if ((u >= 0x3040 && u <= 0x30ff) || (u >= 0x3400 && u <= 0x9fff) || (u >= 0xac00 && u <= 0xd7af))
            return QStringLiteral("cjk");
        if (u >= 0x0400 && u <= 0x04ff)
            return QStringLiteral("cyrillic");
        if (u >= 0x0600 && u <= 0x06ff)
            return QStringLiteral("arabic");
    }
    return QStringLiteral("latin");
}

/** The writing of the person's own language, from the system's. */
QString ownWriting()
{
    switch (QLocale::system().language()) {
    case QLocale::Chinese: case QLocale::Japanese: case QLocale::Korean:
        return QStringLiteral("cjk");
    case QLocale::Russian: case QLocale::Ukrainian: case QLocale::Belarusian: case QLocale::Bulgarian:
    case QLocale::Serbian: case QLocale::Macedonian: case QLocale::Kazakh:
        return QStringLiteral("cyrillic");
    case QLocale::Arabic: case QLocale::Persian: case QLocale::Urdu:
        return QStringLiteral("arabic");
    default:
        return QStringLiteral("latin");
    }
}

QString plain(const QString &html)
{
    return QTextDocumentFragment::fromHtml(html).toPlainText().simplified();
}

QJsonObject entry(const QString &source, const QString &name, const QString &description,
                  const QString &author, qint64 installs, const QString &page, const QString &install)
{
    return QJsonObject{{QStringLiteral("source"), source},
                       {QStringLiteral("name"), name},
                       {QStringLiteral("description"), description},
                       {QStringLiteral("author"), author},
                       {QStringLiteral("installs"), installs},
                       {QStringLiteral("page"), page},
                       {QStringLiteral("install"), install}};
}

} // namespace

Store::Store(UserScripts *scripts, QObject *parent)
    : QObject(parent)
    , m_scripts(scripts)
{
}

QJsonArray Store::parseGreasyFork(const QByteArray &json)
{
    QJsonArray results;
    for (const QJsonValue &value : QJsonDocument::fromJson(json).array()) {
        const QJsonObject script = value.toObject();
        if (script.value(QStringLiteral("deleted")).toBool())
            continue;
        const QString install = script.value(QStringLiteral("code_url")).toString();
        if (!install.endsWith(QLatin1String(".user.js")))
            continue; // libraries and styles are not scripts to add
        const QJsonArray users = script.value(QStringLiteral("users")).toArray();
        results.append(entry(QString::fromLatin1(kGreasyFork),
                             script.value(QStringLiteral("name")).toString(),
                             script.value(QStringLiteral("description")).toString(),
                             users.isEmpty() ? QString() : users.first().toObject().value(QStringLiteral("name")).toString(),
                             qint64(script.value(QStringLiteral("total_installs")).toDouble()),
                             script.value(QStringLiteral("url")).toString(), install));
    }
    return results;
}

QJsonArray Store::parseOpenUserJs(const QByteArray &html)
{
    // One table row per script: its link, name, author, description, then installs.
    static const QRegularExpression row(QStringLiteral(
        R"re(<a href="/scripts/([^/"]+)/([^"]+)" class="tr-link-a"><b>([^<]*)</b>.*?)re"
        R"re(by <a href="/users/[^"]*">([^<]*)</a>\s*(?:<p>([^<]*)</p>)?.*?)re"
        R"re(<td class="text-center td-fit">\s*<p>(\d+)</p>)re"),
        QRegularExpression::DotMatchesEverythingOption);
    QJsonArray results;
    const QString text = QString::fromUtf8(html);
    for (auto it = row.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        const QString user = match.captured(1);
        const QString script = match.captured(2);
        results.append(entry(QString::fromLatin1(kOpenUserJs), plain(match.captured(3)), plain(match.captured(5)),
                             plain(match.captured(4)), match.captured(6).toLongLong(),
                             QStringLiteral("https://openuserjs.org/scripts/%1/%2").arg(user, script),
                             QStringLiteral("https://openuserjs.org/install/%1/%2.user.js").arg(user, script)));
    }
    return results;
}

void Store::reply(QWebEngineUrlRequestJob *job, const QByteArray &type, const QByteArray &data)
{
    auto *buffer = new QBuffer(job);
    buffer->setData(data);
    buffer->open(QIODevice::ReadOnly);
    job->reply(type, buffer);
}

void Store::handle(QWebEngineUrlRequestJob *job)
{
    const QUrl url = job->requestUrl();
    const QString path = url.path();
    const QUrlQuery query(url);
    const QByteArray json = QByteArrayLiteral("application/json");

    if (path == QLatin1String("/api/search")) {
        search(job, query.queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded));
        return;
    }
    if (path == QLatin1String("/api/installed")) {
        QJsonArray names;
        if (m_scripts) {
            for (const UserScript &script : m_scripts->scripts())
                names.append(script.name);
        }
        reply(job, json, QJsonDocument(names).toJson(QJsonDocument::Compact));
        return;
    }
    if (path == QLatin1String("/api/themes") || path == QLatin1String("/api/theme")) {
        if (path == QLatin1String("/api/theme")) {
            Theme::storePalette(query.queryItemValue(QStringLiteral("id")));
            Theme::apply(Theme::storedMode());
            Preferences::instance()->notifyChanged();
        }
        QJsonArray themes;
        for (const Theme::Palette &palette : Theme::palettes()) {
            themes.append(QJsonObject{{QStringLiteral("id"), palette.id},
                                      {QStringLiteral("name"), palette.name},
                                      {QStringLiteral("light"), palette.lightAccent.name()},
                                      {QStringLiteral("dark"), palette.darkAccent.name()},
                                      {QStringLiteral("current"), palette.id == Theme::storedPalette()}});
        }
        reply(job, json, QJsonDocument(themes).toJson(QJsonDocument::Compact));
        return;
    }

    QFile file(QStringLiteral(":/html/store.html"));
    if (!file.open(QIODevice::ReadOnly)) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    QByteArray page = file.readAll();
    // Follow the browser's light or dark, and its accent.
    if (Theme::hasKnownScheme()) {
        page.replace(QByteArrayLiteral("<html lang=\"en\">"),
                     Theme::isDark() ? QByteArrayLiteral("<html lang=\"en\" class=\"dark\">")
                                     : QByteArrayLiteral("<html lang=\"en\" class=\"light\">"));
    }
    page.replace(QByteArrayLiteral("/*SP_ACCENT*/"), Theme::accentCss());
    reply(job, QByteArrayLiteral("text/html; charset=utf-8"), page);
}

void Store::search(QWebEngineUrlRequestJob *job, const QString &query)
{
    if (!m_network)
        m_network = new QNetworkAccessManager(this);

    struct Search {
        QPointer<QWebEngineUrlRequestJob> job;
        QJsonArray results;
        QStringList errors;
        int pending = 2;
        bool answered = false;
    };
    auto state = std::make_shared<Search>();
    state->job = job;

    const auto finish = [this, state] {
        if (state->answered)
            return;
        state->answered = true;
        if (!state->job)
            return; // the page went away
        QList<QJsonValue> sorted(state->results.cbegin(), state->results.cend());
        // Scripts in the person's own writing first, then by installs.
        const QString own = ownWriting();
        const auto foreign = [&own](const QJsonValue &value) {
            const QJsonObject script = value.toObject();
            return writingOf(script.value(QStringLiteral("name")).toString()
                             + script.value(QStringLiteral("description")).toString()) != own;
        };
        std::stable_sort(sorted.begin(), sorted.end(), [&foreign](const QJsonValue &a, const QJsonValue &b) {
            if (foreign(a) != foreign(b))
                return !foreign(a);
            return a.toObject().value(QStringLiteral("installs")).toDouble()
                > b.toObject().value(QStringLiteral("installs")).toDouble();
        });
        QJsonArray results;
        for (const QJsonValue &value : sorted)
            results.append(value);
        reply(state->job, QByteArrayLiteral("application/json"),
              QJsonDocument(QJsonObject{{QStringLiteral("results"), results},
                                        {QStringLiteral("errors"), QJsonArray::fromStringList(state->errors)}})
                  .toJson(QJsonDocument::Compact));
    };
    QTimer::singleShot(kSearchTimeoutMs, this, [state, finish] {
        if (!state->answered)
            state->errors.append(tr("A store took too long to answer."));
        finish();
    });

    const auto fetch = [this, state, finish](const QUrl &url, const QString &source,
                                             QJsonArray (*parse)(const QByteArray &)) {
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SpacePenguin Store"));
        QNetworkReply *reply = m_network->get(request);
        connect(reply, &QNetworkReply::finished, this, [reply, state, finish, source, parse] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                state->errors.append(tr("%1 could not be reached: %2").arg(source, reply->errorString()));
            } else {
                for (const QJsonValue &value : parse(reply->readAll()))
                    state->results.append(value);
            }
            if (--state->pending == 0)
                finish();
        });
    };

    QUrl greasyFork(QStringLiteral("https://api.greasyfork.org/en/scripts.json"));
    QUrlQuery greasyQuery;
    if (!query.trimmed().isEmpty())
        greasyQuery.addQueryItem(QStringLiteral("q"), query.trimmed());
    greasyQuery.addQueryItem(QStringLiteral("sort"), QStringLiteral("total_installs"));
    greasyFork.setQuery(greasyQuery);
    fetch(greasyFork, QString::fromLatin1(kGreasyFork), &Store::parseGreasyFork);

    QUrl openUserJs(QStringLiteral("https://openuserjs.org/"));
    QUrlQuery openQuery;
    if (!query.trimmed().isEmpty())
        openQuery.addQueryItem(QStringLiteral("q"), query.trimmed());
    openQuery.addQueryItem(QStringLiteral("orderBy"), QStringLiteral("installs"));
    openQuery.addQueryItem(QStringLiteral("orderDir"), QStringLiteral("desc"));
    openUserJs.setQuery(openQuery);
    fetch(openUserJs, QString::fromLatin1(kOpenUserJs), &Store::parseOpenUserJs);
}

}

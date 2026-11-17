#include "translator.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <memory>

namespace spacepenguin {

QList<Translator::Service> Translator::services()
{
    return {Service::Google, Service::Bing, Service::DeepL, Service::Yandex, Service::LibreTranslate};
}

QString Translator::key(Service service)
{
    switch (service) {
    case Service::Google: return QStringLiteral("google");
    case Service::Bing: return QStringLiteral("bing");
    case Service::DeepL: return QStringLiteral("deepl");
    case Service::Yandex: return QStringLiteral("yandex");
    case Service::LibreTranslate: return QStringLiteral("libretranslate");
    }
    return QString();
}

Translator::Service Translator::fromKey(const QString &key)
{
    for (const Service service : services()) {
        if (Translator::key(service) == key)
            return service;
    }
    return Service::Google;
}

QString Translator::name(Service service)
{
    switch (service) {
    case Service::Google: return QStringLiteral("Google Translate");
    case Service::Bing: return QStringLiteral("Bing Translator");
    case Service::DeepL: return QStringLiteral("DeepL");
    case Service::Yandex: return QStringLiteral("Yandex Translate");
    case Service::LibreTranslate: return QStringLiteral("LibreTranslate");
    }
    return QString();
}

bool Translator::translatesPages(Service service)
{
    return service == Service::Google || service == Service::LibreTranslate;
}

QString Translator::targetLanguage(const QString &tag)
{
    const QString lower = tag.toLower();
    const QString base = lower.section(QLatin1Char('-'), 0, 0);
    if (base == QLatin1String("zh")) {
        const bool traditional = lower.contains(QLatin1String("hant")) || lower.endsWith(QLatin1String("-tw"))
            || lower.endsWith(QLatin1String("-hk")) || lower.endsWith(QLatin1String("-mo"));
        return traditional ? QStringLiteral("zh-TW") : QStringLiteral("zh-CN");
    }
    return base.isEmpty() ? QStringLiteral("en") : base;
}

QUrl Translator::pageUrl(Service service, const QUrl &url, const QString &target)
{
    const QString scheme = url.scheme().toLower();
    if (service != Service::Google || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        || isTranslatorUrl(url))
        return QUrl();
    const QString language = QString::fromLatin1(QUrl::toPercentEncoding(target));
    return QUrl(QStringLiteral("https://translate.google.com/translate?sl=auto&tl=%1&hl=%1&u=%2")
                    .arg(language, QString::fromLatin1(QUrl::toPercentEncoding(url.toString(QUrl::FullyEncoded)))),
                QUrl::StrictMode);
}

int Translator::textLimit(Service service)
{
    switch (service) {
    case Service::Bing: return 1000;
    case Service::DeepL: return 1500;
    case Service::Google:
    case Service::Yandex:
    case Service::LibreTranslate: return 5000;
    }
    return 1000;
}

QUrl Translator::textUrl(Service service, const QString &text, const QString &target, const QUrl &libreTranslateServer)
{
    QString cut = text.trimmed();
    if (cut.size() > textLimit(service)) {
        cut.truncate(textLimit(service));
        const int space = cut.lastIndexOf(QLatin1Char(' '));
        if (space > textLimit(service) / 2)
            cut.truncate(space);
    }
    const auto encoded = [](const QString &value) { return QString::fromLatin1(QUrl::toPercentEncoding(value)); };
    QString address;
    switch (service) {
    case Service::Google:
        address = QStringLiteral("https://translate.google.com/?sl=auto&tl=%1&op=translate&text=%2")
                      .arg(encoded(target), encoded(cut));
        break;
    case Service::Bing:
        address = QStringLiteral("https://www.bing.com/translator?from=auto-detect&to=%1&text=%2")
                      .arg(encoded(target), encoded(cut));
        break;
    case Service::DeepL: {
        // DeepL reads source/target/text from the fragment, which stays in the
        // browser until its page reads it; slashes in the text are escaped.
        QString escaped = cut;
        escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\")).replace(QLatin1Char('/'), QStringLiteral("\\/"));
        address = QStringLiteral("https://www.deepl.com/translator#auto/%1/%2").arg(encoded(target), encoded(escaped));
        break;
    }
    case Service::Yandex:
        address = QStringLiteral("https://translate.yandex.com/?source_lang=auto&target_lang=%1&text=%2")
                      .arg(encoded(target), encoded(cut));
        break;
    case Service::LibreTranslate: {
        QUrl server = libreTranslateServer;
        if (!server.isValid() || (server.scheme() != QLatin1String("https") && server.scheme() != QLatin1String("http")))
            return QUrl();
        server.setPath(QStringLiteral("/"));
        server.setQuery(QStringLiteral("source=auto&target=%1&q=%2").arg(encoded(target), encoded(cut)));
        return server;
    }
    }
    return QUrl(address, QUrl::StrictMode);
}

bool Translator::isTranslatorUrl(const QUrl &url)
{
    const QString host = url.host().toLower();
    return host.startsWith(QLatin1String("translate.google.")) || host.endsWith(QLatin1String(".translate.goog"))
        || host == QLatin1String("translate.goog")
        || ((host == QLatin1String("www.bing.com") || host == QLatin1String("bing.com"))
            && url.path().startsWith(QLatin1String("/translator")))
        || host == QLatin1String("www.deepl.com") || host == QLatin1String("deepl.com")
        || host.startsWith(QLatin1String("translate.yandex."));
}

void Translator::translateTexts(QNetworkAccessManager *network, const QUrl &server, const QString &apiKey,
                                const QStringList &texts, const QString &target,
                                std::function<void(QStringList, QString)> done)
{
    if (!server.isValid() || (server.scheme() != QLatin1String("https") && server.scheme() != QLatin1String("http"))) {
        done({}, QCoreApplication::translate("Translator", "Set the address of a LibreTranslate server in Settings."));
        return;
    }
    if (texts.isEmpty()) {
        done({}, QString());
        return;
    }
    // In batches of a few thousand characters, the limit public servers set;
    // one after the other, so a long page does not flood the server.
    QList<QStringList> batches(1);
    int size = 0;
    for (const QString &text : texts) {
        if (!batches.last().isEmpty() && size + text.size() > 4000) {
            batches.append(QStringList());
            size = 0;
        }
        batches.last() << text;
        size += text.size();
    }
    struct Job {
        QList<QStringList> batches;
        QStringList translations;
        int next = 0;
    };
    auto job = std::make_shared<Job>();
    job->batches = batches;
    QUrl endpoint = server;
    endpoint.setPath(QStringLiteral("/translate"));
    endpoint.setQuery(QString());
    auto step = std::make_shared<std::function<void()>>();
    *step = [network, endpoint, apiKey, target, done, job, step] {
        if (job->next >= job->batches.size()) {
            done(job->translations, QString());
            *step = nullptr; // breaks the cycle
            return;
        }
        const QStringList batch = job->batches[job->next++];
        QJsonObject body{{QStringLiteral("q"), QJsonArray::fromStringList(batch)},
                         {QStringLiteral("source"), QStringLiteral("auto")},
                         {QStringLiteral("target"), target},
                         {QStringLiteral("format"), QStringLiteral("text")}};
        if (!apiKey.isEmpty())
            body.insert(QStringLiteral("api_key"), apiKey);
        QNetworkRequest request(endpoint);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setTransferTimeout(30000);
        QNetworkReply *reply = network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        QObject::connect(reply, &QNetworkReply::finished, reply, [reply, batch, done, job, step] {
            reply->deleteLater();
            const QJsonObject answer = QJsonDocument::fromJson(reply->read(4 * 1024 * 1024)).object();
            const QJsonArray translated = answer.value(QStringLiteral("translatedText")).toArray();
            if (reply->error() != QNetworkReply::NoError || translated.size() != batch.size()) {
                QString message = answer.value(QStringLiteral("error")).toString();
                if (message.isEmpty())
                    message = reply->errorString();
                done({}, QCoreApplication::translate("Translator", "LibreTranslate could not translate the page: %1")
                             .arg(message.left(200)));
                *step = nullptr;
                return;
            }
            for (const QJsonValue &value : translated)
                job->translations << value.toString();
            (*step)();
        });
    };
    (*step)();
}

QString Translator::collectScript()
{
    // The page's visible text, node by node, kept in this world for applyScript.
    return QStringLiteral(R"JS((() => {
        const skip = new Set(['SCRIPT', 'STYLE', 'NOSCRIPT', 'TEXTAREA', 'CODE', 'SVG', 'TEMPLATE']);
        const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, {
            acceptNode(node) {
                for (let e = node.parentElement; e; e = e.parentElement)
                    if (skip.has(e.tagName) || e.isContentEditable) return NodeFilter.FILTER_REJECT;
                return /\p{L}/u.test(node.nodeValue) ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_REJECT;
            }
        });
        const nodes = [], texts = [];
        let total = 0;
        while (walker.nextNode() && nodes.length < 3000 && total < 200000) {
            const text = walker.currentNode.nodeValue.trim();
            nodes.push(walker.currentNode);
            texts.push(text);
            total += text.length;
        }
        window.__spacepenguinTranslateNodes = nodes;
        if (document.title) texts.push(document.title);
        return texts;
    })())JS");
}

QString Translator::applyScript(const QStringList &translations)
{
    const QString list = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(translations)).toJson(QJsonDocument::Compact));
    return QStringLiteral(R"JS(((translations) => {
        const nodes = window.__spacepenguinTranslateNodes || [];
        nodes.forEach((node, i) => {
            if (typeof translations[i] !== 'string') return;
            const lead = node.nodeValue.match(/^\s*/)[0], trail = node.nodeValue.match(/\s*$/)[0];
            node.nodeValue = lead + translations[i] + trail;
        });
        if (translations.length > nodes.length && document.title) document.title = translations[nodes.length];
        return nodes.length;
    })(%1))JS").arg(list);
}

}

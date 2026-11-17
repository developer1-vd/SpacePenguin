#include "crypto.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlSchemeHandler>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>

namespace spacepenguin {

/** The page's console is how its promises report back. */
class CryptoPage : public QWebEnginePage
{
public:
    CryptoPage(QWebEngineProfile *profile, Crypto *crypto)
        : QWebEnginePage(profile, crypto), m_crypto(crypto) {}

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel, const QString &message, int,
                                  const QString &sourceId) override
    {
        // Only this page's own script, never anything else it could be shown.
        if (sourceId.isEmpty() || sourceId.startsWith(QLatin1String("sp://crypto")))
            m_crypto->deliver(message);
    }
    bool acceptNavigationRequest(const QUrl &url, NavigationType, bool) override
    {
        return url == QUrl(QStringLiteral("sp://crypto/"));
    }

private:
    Crypto *m_crypto;
};

namespace {

/** Serves the one empty page, so it has a real, secure origin (sp://crypto). */
class CryptoSchemeHandler : public QWebEngineUrlSchemeHandler
{
public:
    using QWebEngineUrlSchemeHandler::QWebEngineUrlSchemeHandler;
    void requestStarted(QWebEngineUrlRequestJob *job) override
    {
        if (job->requestUrl() != QUrl(QStringLiteral("sp://crypto/"))) {
            job->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
        }
        auto *body = new QBuffer(job);
        body->setData(QByteArrayLiteral("<!doctype html><meta charset=utf-8><title>crypto</title>"));
        body->open(QIODevice::ReadOnly);
        job->reply(QByteArrayLiteral("text/html"), body);
    }
};

const char kScript[] = R"JS(
const b64 = (bytes) => btoa(String.fromCharCode(...new Uint8Array(bytes)));
const unb64 = (text) => Uint8Array.from(atob(text), c => c.charCodeAt(0));
async function key(password, salt, iterations) {
    const material = await crypto.subtle.importKey('raw', new TextEncoder().encode(password), 'PBKDF2', false, ['deriveKey']);
    return crypto.subtle.deriveKey({name: 'PBKDF2', hash: 'SHA-256', salt, iterations}, material,
                                   {name: 'AES-GCM', length: 256}, false, ['encrypt', 'decrypt']);
}
const operations = {
    async encrypt(a) {
        const salt = crypto.getRandomValues(new Uint8Array(16)), iv = crypto.getRandomValues(new Uint8Array(12));
        const data = await crypto.subtle.encrypt({name: 'AES-GCM', iv}, await key(a.password, salt, a.iterations), unb64(a.data));
        return JSON.stringify({format: 'spacepenguin-encrypted', version: 1, kdf: 'PBKDF2-SHA256', iterations: a.iterations,
                               salt: b64(salt), cipher: 'AES-256-GCM', iv: b64(iv), data: b64(data)});
    },
    async decrypt(a) {
        const e = JSON.parse(a.envelope);
        if (e.format !== 'spacepenguin-encrypted' || e.version !== 1 || e.cipher !== 'AES-256-GCM' || e.kdf !== 'PBKDF2-SHA256'
            || !(e.iterations >= 100000 && e.iterations <= 10000000))
            throw new Error('format');
        const plain = await crypto.subtle.decrypt({name: 'AES-GCM', iv: unb64(e.iv)},
                                                  await key(a.password, unb64(e.salt), e.iterations), unb64(e.data));
        return b64(plain);
    },
    async verifier(a) {
        const material = await crypto.subtle.importKey('raw', new TextEncoder().encode(a.password), 'PBKDF2', false, ['deriveBits']);
        return b64(await crypto.subtle.deriveBits({name: 'PBKDF2', hash: 'SHA-256', salt: unb64(a.salt), iterations: a.iterations},
                                                  material, 256));
    },
};
window.spacepenguinCrypto = (id, operation, args) => {
    operations[operation](args).then(result => console.log(JSON.stringify({id, result})),
                                      error => console.log(JSON.stringify({id, error: String(error && error.name || error)})));
};
)JS";

} // namespace

Crypto *Crypto::instance()
{
    static Crypto *crypto = new Crypto(qApp);
    return crypto;
}

Crypto::Crypto(QObject *parent)
    : QObject(parent)
{
}

bool Crypto::isEnvelope(const QByteArray &data)
{
    return QJsonDocument::fromJson(data).object().value(QStringLiteral("format")).toString()
        == QLatin1String("spacepenguin-encrypted");
}

void Crypto::run(const QString &operation, const QJsonObject &arguments, Done done)
{
    if (!m_page) {
        // Off the record and with nothing to load: a page only for WebCrypto, in
        // a secure context (sp: is registered as a secure scheme).
        m_profile = new QWebEngineProfile(this);
        m_profile->installUrlSchemeHandler(QByteArrayLiteral("sp"), new CryptoSchemeHandler(m_profile));
        m_page = new CryptoPage(m_profile, this);
        connect(m_page, &QWebEnginePage::loadFinished, this, [this](bool) {
            m_page->runJavaScript(QString::fromUtf8(kScript), QWebEngineScript::ApplicationWorld, [this](const QVariant &) {
                m_ready = true;
                for (const auto &call : std::exchange(m_waiting, {}))
                    call();
            });
        });
        m_page->load(QUrl(QStringLiteral("sp://crypto/")));
    }
    const int id = m_next++;
    m_pending.insert(id, std::move(done));
    // The operation is one of this file's own names; the arguments go in as JSON.
    const QString call = QStringLiteral("spacepenguinCrypto(%1, '%2', %3)")
                             .arg(QString::number(id), operation,
                                  QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact)));
    const auto start = [this, call] { m_page->runJavaScript(call, QWebEngineScript::ApplicationWorld); };
    if (m_ready)
        start();
    else
        m_waiting.append(start);
}

void Crypto::deliver(const QString &message)
{
    const QJsonObject answer = QJsonDocument::fromJson(message.toUtf8()).object();
    const int id = answer.value(QStringLiteral("id")).toInt();
    const Done done = m_pending.take(id);
    if (!done)
        return;
    if (answer.contains(QStringLiteral("error")))
        done({}, answer.value(QStringLiteral("error")).toString());
    else
        done(answer.value(QStringLiteral("result")).toString().toUtf8(), QString());
}

void Crypto::encrypt(const QByteArray &plain, const QString &password, Done done)
{
    run(QStringLiteral("encrypt"),
        {{QStringLiteral("data"), QString::fromLatin1(plain.toBase64())},
         {QStringLiteral("password"), password},
         {QStringLiteral("iterations"), kIterations}},
        std::move(done));
}

void Crypto::decrypt(const QByteArray &envelope, const QString &password, Done done)
{
    run(QStringLiteral("decrypt"),
        {{QStringLiteral("envelope"), QString::fromUtf8(envelope)}, {QStringLiteral("password"), password}},
        [done = std::move(done)](const QByteArray &result, const QString &error) {
            if (!error.isEmpty()) {
                done({}, error == QLatin1String("OperationError")
                             ? QCoreApplication::translate("Crypto", "The password is wrong, or the file was changed.")
                             : QCoreApplication::translate("Crypto", "This is not a file SpacePenguin encrypted."));
                return;
            }
            done(QByteArray::fromBase64(result), QString());
        });
}

void Crypto::deriveVerifier(const QString &password, const QByteArray &salt, Done done)
{
    run(QStringLiteral("verifier"),
        {{QStringLiteral("password"), password},
         {QStringLiteral("salt"), QString::fromLatin1(salt)},
         {QStringLiteral("iterations"), kIterations}},
        std::move(done));
}

}

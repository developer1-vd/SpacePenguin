#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

class QJsonObject;
class QWebEnginePage;
class QWebEngineProfile;

namespace spacepenguin {

/**
 * Password-based encryption: AES-256-GCM with a key from PBKDF2-HMAC-SHA256
 * (600,000 rounds, a random 16-byte salt and 12-byte nonce for each file).
 *
 * The work is done by Chromium's WebCrypto (BoringSSL), in a page of its own
 * that loads nothing, so SpacePenguin carries no cryptography of its own and
 * every platform uses the same well-tested code.
 *
 * Encrypted data is a small JSON envelope: {"format": "spacepenguin-encrypted",
 * "version": 1, "kdf": "PBKDF2-SHA256", "iterations", "salt", "cipher":
 * "AES-256-GCM", "iv", "data"} with base64 fields. A wrong password or changed
 * data fails to decrypt: GCM checks it.
 */
class Crypto : public QObject
{
    Q_OBJECT

public:
    using Done = std::function<void(QByteArray result, QString error)>;

    static Crypto *instance();

    static constexpr int kIterations = 600000;

    void encrypt(const QByteArray &plain, const QString &password, Done done);
    void decrypt(const QByteArray &envelope, const QString &password, Done done);
    /** PBKDF2 of @p password with @p salt (base64), as base64: to check a password later. */
    void deriveVerifier(const QString &password, const QByteArray &salt, Done done);

    /** Whether @p data looks like an envelope encrypt() wrote. */
    static bool isEnvelope(const QByteArray &data);

private:
    explicit Crypto(QObject *parent = nullptr);
    void run(const QString &operation, const QJsonObject &arguments, Done done);
    void deliver(const QString &message);
    friend class CryptoPage;

    QWebEngineProfile *m_profile = nullptr;
    QWebEnginePage *m_page = nullptr;
    bool m_ready = false;
    QList<std::function<void()>> m_waiting;
    QHash<int, Done> m_pending;
    int m_next = 1;
};

}

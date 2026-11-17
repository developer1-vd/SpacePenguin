#include <QtTest>

#include "crypto.h"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWebEngineUrlScheme>

using namespace spacepenguin;

class TestCrypto : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
    void wrongPasswordFails();
    void changedDataFails();
    void eachFileHasItsOwnSaltAndNonce();
    void verifierIsStable();
};

static std::pair<QByteArray, QString> wait(std::function<void(Crypto::Done)> start)
{
    std::pair<QByteArray, QString> out;
    bool done = false;
    start([&](const QByteArray &result, const QString &error) {
        out = {result, error};
        done = true;
    });
    QTest::qWaitFor([&] { return done; }, 60000);
    return out;
}

void TestCrypto::roundTrip()
{
    const QByteArray plain = QByteArrayLiteral("logins: héllo \x00\x01 world").repeated(50);
    const auto [envelope, error] = wait([&](Crypto::Done d) { Crypto::instance()->encrypt(plain, QStringLiteral("correct horse"), d); });
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(Crypto::isEnvelope(envelope));
    QVERIFY(!envelope.contains("héllo"));
    const QJsonObject json = QJsonDocument::fromJson(envelope).object();
    QCOMPARE(json.value(QStringLiteral("cipher")).toString(), QStringLiteral("AES-256-GCM"));
    QCOMPARE(json.value(QStringLiteral("iterations")).toInt(), Crypto::kIterations);
    const auto [back, error2] = wait([&](Crypto::Done d) { Crypto::instance()->decrypt(envelope, QStringLiteral("correct horse"), d); });
    QVERIFY2(error2.isEmpty(), qPrintable(error2));
    QCOMPARE(back, plain);
}

void TestCrypto::wrongPasswordFails()
{
    const auto [envelope, error] = wait([&](Crypto::Done d) { Crypto::instance()->encrypt("secret", QStringLiteral("one password"), d); });
    QVERIFY(error.isEmpty());
    const auto [back, error2] = wait([&](Crypto::Done d) { Crypto::instance()->decrypt(envelope, QStringLiteral("another"), d); });
    QVERIFY(back.isEmpty());
    QVERIFY(!error2.isEmpty());
}

void TestCrypto::changedDataFails()
{
    const auto [envelope, error] = wait([&](Crypto::Done d) { Crypto::instance()->encrypt("secret data", QStringLiteral("pw123456"), d); });
    QJsonObject json = QJsonDocument::fromJson(envelope).object();
    QByteArray data = QByteArray::fromBase64(json.value(QStringLiteral("data")).toString().toLatin1());
    data[0] = char(data[0] ^ 1);
    json.insert(QStringLiteral("data"), QString::fromLatin1(data.toBase64()));
    const auto [back, error2] = wait([&](Crypto::Done d) {
        Crypto::instance()->decrypt(QJsonDocument(json).toJson(), QStringLiteral("pw123456"), d);
    });
    QVERIFY(back.isEmpty());
    QVERIFY(!error2.isEmpty());
    // Too few rounds is refused rather than tried.
    json.insert(QStringLiteral("iterations"), 1);
    const auto [weak, error3] = wait([&](Crypto::Done d) {
        Crypto::instance()->decrypt(QJsonDocument(json).toJson(), QStringLiteral("pw123456"), d);
    });
    QVERIFY(!error3.isEmpty());
}

void TestCrypto::eachFileHasItsOwnSaltAndNonce()
{
    const auto [a, e1] = wait([&](Crypto::Done d) { Crypto::instance()->encrypt("same", QStringLiteral("same password"), d); });
    const auto [b, e2] = wait([&](Crypto::Done d) { Crypto::instance()->encrypt("same", QStringLiteral("same password"), d); });
    const QJsonObject ja = QJsonDocument::fromJson(a).object(), jb = QJsonDocument::fromJson(b).object();
    QVERIFY(ja.value(QStringLiteral("salt")) != jb.value(QStringLiteral("salt")));
    QVERIFY(ja.value(QStringLiteral("iv")) != jb.value(QStringLiteral("iv")));
}

void TestCrypto::verifierIsStable()
{
    const QByteArray salt = QByteArrayLiteral("AAAAAAAAAAAAAAAAAAAAAA==");
    const auto [one, e1] = wait([&](Crypto::Done d) { Crypto::instance()->deriveVerifier(QStringLiteral("pw"), salt, d); });
    const auto [two, e2] = wait([&](Crypto::Done d) { Crypto::instance()->deriveVerifier(QStringLiteral("pw"), salt, d); });
    const auto [other, e3] = wait([&](Crypto::Done d) { Crypto::instance()->deriveVerifier(QStringLiteral("px"), salt, d); });
    QVERIFY(e1.isEmpty());
    QCOMPARE(one, two);
    QVERIFY(one != other);
    QCOMPARE(QByteArray::fromBase64(one).size(), 32);
}

int main(int argc, char *argv[])
{
    QWebEngineUrlScheme scheme(QByteArrayLiteral("sp"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme);
    QWebEngineUrlScheme::registerScheme(scheme);
    QApplication app(argc, argv);
    TestCrypto test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_crypto.moc"

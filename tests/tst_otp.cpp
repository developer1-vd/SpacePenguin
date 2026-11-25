#include <QtTest>

#include "otp.h"

using namespace spacepenguin;
using Algorithm = OtpAccount::Algorithm;

class TestOtp : public QObject
{
    Q_OBJECT

private slots:
    void base32();
    void base32RejectsBadKeys();
    void hotpRfc4226();
    void totpRfc6238_data();
    void totpRfc6238();
    void secondsLeft();
    void parsesLinks();
    void rejectsBadLinks_data();
    void rejectsBadLinks();
    void linkRoundTrip();
    void keyByHand();
    void matchesSites();
};

void TestOtp::base32()
{
    // RFC 4648 section 10.
    QCOMPARE(Otp::base32Encode("foobar"), QStringLiteral("MZXW6YTBOI"));
    QCOMPARE(Otp::base32Decode(QStringLiteral("MZXW6YTBOI======")), QByteArray("foobar"));
    QCOMPARE(Otp::base32Decode(QStringLiteral("MZXW6YQ=")), QByteArray("foob"));
    // As sites show keys: lower case, in groups.
    QCOMPARE(Otp::base32Decode(QStringLiteral("mzxw 6ytb oi")), QByteArray("foobar"));
    QCOMPARE(Otp::base32Decode(QStringLiteral("MZXW-6YTB-OI")), QByteArray("foobar"));
    const QByteArray key = QByteArrayLiteral("\x00\x01\xfe\xff 12345678901234567890");
    QCOMPARE(Otp::base32Decode(Otp::base32Encode(key)), key);
}

void TestOtp::base32RejectsBadKeys()
{
    QVERIFY(Otp::base32Decode(QStringLiteral("MZXW0YTB")).isEmpty()); // 0 is not base32
    QVERIFY(Otp::base32Decode(QStringLiteral("MZXW1YTB")).isEmpty()); // nor is 1
    QVERIFY(Otp::base32Decode(QStringLiteral("MZXW6YTB!")).isEmpty());
    QVERIFY(Otp::base32Decode(QStringLiteral("MZXW6YTBO")).isEmpty()); // 9 characters: no whole bytes
}

void TestOtp::hotpRfc4226()
{
    // RFC 4226 appendix D.
    const QByteArray secret("12345678901234567890");
    const QStringList expected = {"755224", "287082", "359152", "969429", "338314",
                                  "254676", "287922", "162583", "399871", "520489"};
    for (int counter = 0; counter < expected.size(); ++counter)
        QCOMPARE(Otp::hotp(secret, quint64(counter)), expected.at(counter));
}

void TestOtp::totpRfc6238_data()
{
    QTest::addColumn<qint64>("time");
    QTest::addColumn<int>("algorithm");
    QTest::addColumn<QString>("code");
    // RFC 6238 appendix B.
    const struct {
        qint64 time;
        const char *sha1, *sha256, *sha512;
    } vectors[] = {
        {59, "94287082", "46119246", "90693936"},
        {1111111109, "07081804", "68084774", "25091201"},
        {1111111111, "14050471", "67062674", "99943326"},
        {1234567890, "89005924", "91819424", "93441116"},
        {2000000000, "69279037", "90698825", "38618901"},
        {20000000000, "65353130", "77737706", "47863826"},
    };
    for (const auto &v : vectors) {
        QTest::addRow("sha1 %lld", v.time) << v.time << int(Algorithm::Sha1) << QString::fromLatin1(v.sha1);
        QTest::addRow("sha256 %lld", v.time) << v.time << int(Algorithm::Sha256) << QString::fromLatin1(v.sha256);
        QTest::addRow("sha512 %lld", v.time) << v.time << int(Algorithm::Sha512) << QString::fromLatin1(v.sha512);
    }
}

void TestOtp::totpRfc6238()
{
    QFETCH(qint64, time);
    QFETCH(int, algorithm);
    QFETCH(QString, code);
    // Each algorithm has its own key length in the RFC.
    QByteArray secret("12345678901234567890");
    if (Algorithm(algorithm) == Algorithm::Sha256)
        secret = "12345678901234567890123456789012";
    else if (Algorithm(algorithm) == Algorithm::Sha512)
        secret = "1234567890123456789012345678901234567890123456789012345678901234";
    QCOMPARE(Otp::totp(secret, time, 30, 8, Algorithm(algorithm)), code);
}

void TestOtp::secondsLeft()
{
    OtpAccount account;
    account.period = 30;
    QCOMPARE(Otp::secondsLeft(account, 0), 30);
    QCOMPARE(Otp::secondsLeft(account, 29), 1);
    QCOMPARE(Otp::secondsLeft(account, 30), 30);
    QCOMPARE(Otp::secondsLeft(account, 1234567890), 30 - 1234567890 % 30);
}

void TestOtp::parsesLinks()
{
    QString error;
    OtpAccount a = Otp::parseUri(
        QStringLiteral("otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example"), &error);
    QVERIFY2(a.isValid(), qPrintable(error));
    QCOMPARE(a.type, OtpAccount::Type::Totp);
    QCOMPARE(a.issuer, QStringLiteral("Example"));
    QCOMPARE(a.name, QStringLiteral("alice@google.com"));
    QCOMPARE(a.secret, QByteArray("Hello!\xde\xad\xbe\xef"));
    QCOMPARE(a.digits, 6);
    QCOMPARE(a.period, 30);
    QCOMPARE(a.algorithm, Algorithm::Sha1);

    // Issuer only in the label, percent-encoded, with every option.
    a = Otp::parseUri(QStringLiteral("otpauth://totp/ACME%20Co:john.doe%40email.com?"
                                     "secret=HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ&algorithm=SHA256&digits=8&period=60"),
                      &error);
    QVERIFY2(a.isValid(), qPrintable(error));
    QCOMPARE(a.issuer, QStringLiteral("ACME Co"));
    QCOMPARE(a.name, QStringLiteral("john.doe@email.com"));
    QCOMPARE(a.algorithm, Algorithm::Sha256);
    QCOMPARE(a.digits, 8);
    QCOMPARE(a.period, 60);

    // No issuer at all, and HOTP with a counter.
    a = Otp::parseUri(QStringLiteral("otpauth://hotp/bob?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&counter=5"), &error);
    QVERIFY2(a.isValid(), qPrintable(error));
    QCOMPARE(a.type, OtpAccount::Type::Hotp);
    QVERIFY(a.issuer.isEmpty());
    QCOMPARE(a.name, QStringLiteral("bob"));
    QCOMPARE(a.counter, quint64(5));
    QCOMPARE(Otp::code(a, 0), QStringLiteral("254676")); // RFC 4226, counter 5

    // Upper-case scheme and type, as some QR codes have.
    QVERIFY(Otp::parseUri(QStringLiteral("OTPAUTH://TOTP/x?secret=JBSWY3DPEHPK3PXP")).isValid());
}

void TestOtp::rejectsBadLinks_data()
{
    QTest::addColumn<QString>("uri");
    QTest::newRow("web link") << QStringLiteral("https://example.com/?secret=JBSWY3DPEHPK3PXP");
    QTest::newRow("no secret") << QStringLiteral("otpauth://totp/x?issuer=y");
    QTest::newRow("bad secret") << QStringLiteral("otpauth://totp/x?secret=not-base32!");
    QTest::newRow("short secret") << QStringLiteral("otpauth://totp/x?secret=MZXQ");
    QTest::newRow("steam") << QStringLiteral("otpauth://steam/x?secret=JBSWY3DPEHPK3PXP");
    QTest::newRow("md5") << QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&algorithm=MD5");
    QTest::newRow("4 digits") << QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&digits=4");
    QTest::newRow("12 digits") << QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&digits=12");
    QTest::newRow("period 0") << QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&period=0");
    QTest::newRow("period text") << QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&period=abc");
    QTest::newRow("bad counter") << QStringLiteral("otpauth://hotp/x?secret=JBSWY3DPEHPK3PXP&counter=-1");
    QTest::newRow("empty") << QString();
}

void TestOtp::rejectsBadLinks()
{
    QFETCH(QString, uri);
    QString error;
    QVERIFY(!Otp::parseUri(uri, &error).isValid());
    QVERIFY(!error.isEmpty());
}

void TestOtp::linkRoundTrip()
{
    OtpAccount a;
    a.issuer = QStringLiteral("Penguin & Co: Ice/Snow");
    a.name = QStringLiteral("pingu@example.com");
    a.secret = QByteArray("12345678901234567890");
    a.algorithm = Algorithm::Sha512;
    a.digits = 8;
    a.period = 45;
    QString error;
    const OtpAccount b = Otp::parseUri(Otp::toUri(a), &error);
    QVERIFY2(b.isValid(), qPrintable(error));
    QCOMPARE(b.issuer, a.issuer);
    QCOMPARE(b.name, a.name);
    QCOMPARE(b.secret, a.secret);
    QCOMPARE(b.algorithm, a.algorithm);
    QCOMPARE(b.digits, a.digits);
    QCOMPARE(b.period, a.period);

    a.type = OtpAccount::Type::Hotp;
    a.counter = 42;
    QCOMPARE(Otp::parseUri(Otp::toUri(a)).counter, quint64(42));
}

void TestOtp::keyByHand()
{
    QString error;
    OtpAccount a = Otp::fromKey(QStringLiteral(" GitHub "), QStringLiteral("pingu"), QStringLiteral("jbsw y3dp ehpk 3pxp"), &error);
    QVERIFY2(a.isValid(), qPrintable(error));
    QCOMPARE(a.issuer, QStringLiteral("GitHub"));
    QCOMPARE(a.secret, QByteArray("Hello!\xde\xad\xbe\xef"));

    QVERIFY(!Otp::fromKey(QString(), QString(), QStringLiteral("hello world 1"), &error).isValid());
    QVERIFY(!error.isEmpty());
    QVERIFY(!Otp::fromKey(QString(), QString(), QString(), &error).isValid());
    QVERIFY(!error.isEmpty());
}

void TestOtp::matchesSites()
{
    OtpAccount github;
    github.issuer = QStringLiteral("GitHub");
    QVERIFY(Otp::matchesSite(github, QUrl(QStringLiteral("https://github.com/sessions/two-factor"))));
    QVERIFY(Otp::matchesSite(github, QUrl(QStringLiteral("https://www.github.com/"))));
    QVERIFY(!Otp::matchesSite(github, QUrl(QStringLiteral("https://gitlab.com/"))));
    QVERIFY(!Otp::matchesSite(github, QUrl(QStringLiteral("https://github-login.evil.example/"))));
    // A top-level domain is not a site name.
    OtpAccount com;
    com.issuer = QStringLiteral("com");
    QVERIFY(!Otp::matchesSite(com, QUrl(QStringLiteral("https://example.com/"))));

    OtpAccount google;
    google.issuer = QStringLiteral("Google");
    QVERIFY(Otp::matchesSite(google, QUrl(QStringLiteral("https://accounts.google.com/signin"))));

    // Where it was added counts, subdomains included, whatever its name.
    OtpAccount added;
    added.issuer = QStringLiteral("Corp SSO");
    added.site = QStringLiteral("https://login.example.org");
    QVERIFY(Otp::matchesSite(added, QUrl(QStringLiteral("https://login.example.org/mfa"))));
    QVERIFY(Otp::matchesSite(added, QUrl(QStringLiteral("https://sso.login.example.org/"))));
    QVERIFY(!Otp::matchesSite(added, QUrl(QStringLiteral("https://example.net/"))));
}

QTEST_GUILESS_MAIN(TestOtp)
#include "tst_otp.moc"

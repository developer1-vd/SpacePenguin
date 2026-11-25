#include <QtTest>

#include "authenticator.h"
#include "authenticatorpage.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace spacepenguin;

class TestAuthenticator : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void keepsAccounts();
    void sameKeyReplaces();
    void hotpMovesOn();
    void onlyItsPageMayAsk();
    void fillsOnlyDigits();

private:
    static OtpAccount github();
};

void TestAuthenticator::initTestCase()
{
    // A file, not the desktop's keyring, so the test runs anywhere.
    qputenv("SPACEPENGUIN_NO_KEYRING", "1");
}

OtpAccount TestAuthenticator::github()
{
    OtpAccount account = Otp::parseUri(QStringLiteral("otpauth://totp/GitHub:pingu?secret=JBSWY3DPEHPK3PXP&issuer=GitHub"));
    account.site = QStringLiteral("https://github.com");
    return account;
}

void TestAuthenticator::keepsAccounts()
{
    QTemporaryDir dir;
    QString id;
    {
        AuthenticatorStore store(dir.path());
        QSignalSpy changed(&store, &AuthenticatorStore::changed);
        id = store.add(github());
        QVERIFY(!id.isEmpty());
        QCOMPARE(changed.count(), 1);
        QVERIFY(!store.add(OtpAccount()).size()); // no key, not added
        QCOMPARE(store.accounts().size(), 1);
    }
#ifndef Q_OS_WIN
    // Only the user may read the keys.
    const QString path = dir.filePath(QStringLiteral("authenticator.json"));
    QCOMPARE(QFile::permissions(path) & (QFileDevice::ReadGroup | QFileDevice::ReadOther), QFileDevice::Permissions());
#endif
    AuthenticatorStore again(dir.path());
    QCOMPARE(again.accounts().size(), 1);
    const OtpAccount account = again.account(id);
    QCOMPARE(account.issuer, QStringLiteral("GitHub"));
    QCOMPARE(account.name, QStringLiteral("pingu"));
    QCOMPARE(account.secret, github().secret);
    QCOMPARE(account.site, QStringLiteral("https://github.com"));
    QCOMPARE(again.accountsFor(QUrl(QStringLiteral("https://github.com/login"))).size(), 1);
    QCOMPARE(again.accountsFor(QUrl(QStringLiteral("https://example.com/"))).size(), 0);

    again.rename(id, QStringLiteral("Work GitHub"), QStringLiteral("pingu"));
    QCOMPARE(AuthenticatorStore(dir.path()).account(id).issuer, QStringLiteral("Work GitHub"));
    again.remove(id);
    QVERIFY(AuthenticatorStore(dir.path()).accounts().isEmpty());
}

void TestAuthenticator::sameKeyReplaces()
{
    QTemporaryDir dir;
    AuthenticatorStore store(dir.path());
    const QString id = store.add(github());
    OtpAccount renamed = github();
    renamed.name = QStringLiteral("pingu2");
    renamed.site.clear();
    QCOMPARE(store.add(renamed), id);
    QCOMPARE(store.accounts().size(), 1);
    QCOMPARE(store.account(id).name, QStringLiteral("pingu2"));
    QCOMPARE(store.account(id).site, QStringLiteral("https://github.com")); // kept
}

void TestAuthenticator::hotpMovesOn()
{
    QTemporaryDir dir;
    QString id;
    {
        AuthenticatorStore store(dir.path());
        id = store.add(Otp::parseUri(QStringLiteral("otpauth://hotp/x?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ")));
        QCOMPARE(store.takeCode(id, 0), QStringLiteral("755224")); // RFC 4226, counters 0 and 1
        QCOMPARE(store.takeCode(id, 0), QStringLiteral("287082"));
    }
    // The counter is kept: a code is never given twice.
    QCOMPARE(AuthenticatorStore(dir.path()).takeCode(id, 0), QStringLiteral("359152"));
}

void TestAuthenticator::onlyItsPageMayAsk()
{
    QVERIFY(AuthenticatorPage::isAllowedCaller(QUrl(QStringLiteral("sp://authenticator/"))));
    QVERIFY(!AuthenticatorPage::isAllowedCaller(QUrl(QStringLiteral("https://github.com/"))));
    QVERIFY(!AuthenticatorPage::isAllowedCaller(QUrl(QStringLiteral("sp://logins/"))));
    QVERIFY(!AuthenticatorPage::isAllowedCaller(QUrl(QStringLiteral("sp://newtab/"))));
    QVERIFY(!AuthenticatorPage::isAllowedCaller(QUrl()));
}

void TestAuthenticator::fillsOnlyDigits()
{
    QVERIFY(AuthenticatorStore::fillScript(QStringLiteral("123456")).contains(QLatin1String("('123456')")));
    QCOMPARE(AuthenticatorStore::fillScript(QStringLiteral("12345'); alert(1); ('")), QStringLiteral("false;"));
    QCOMPARE(AuthenticatorStore::fillScript(QString()), QStringLiteral("false;"));
}

QTEST_GUILESS_MAIN(TestAuthenticator)
#include "tst_authenticator.moc"

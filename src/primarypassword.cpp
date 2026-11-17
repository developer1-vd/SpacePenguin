#include "primarypassword.h"

#include "crypto.h"

#include <QCoreApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QRandomGenerator>
#include <QSettings>

namespace spacepenguin::PrimaryPassword {
namespace {

bool s_unlocked = false;
const QString kSalt = QStringLiteral("logins/primaryPasswordSalt");
const QString kVerifier = QStringLiteral("logins/primaryPasswordVerifier");

QString tr(const char *text)
{
    return QCoreApplication::translate("PrimaryPassword", text);
}

QString ask(QWidget *parent, const QString &title, const QString &label, bool *ok)
{
    return QInputDialog::getText(parent, title, label, QLineEdit::Password, QString(), ok);
}

/** Calls @p done with whether @p password is the primary password. */
void check(const QString &password, std::function<void(bool)> done)
{
    const QSettings settings;
    const QByteArray salt = settings.value(kSalt).toByteArray();
    const QByteArray expected = settings.value(kVerifier).toByteArray();
    Crypto::instance()->deriveVerifier(password, salt, [expected, done](const QByteArray &verifier, const QString &error) {
        // Compared in full, whatever differs first.
        bool same = error.isEmpty() && verifier.size() == expected.size();
        unsigned char difference = 0;
        for (qsizetype i = 0; same && i < verifier.size(); ++i)
            difference |= static_cast<unsigned char>(verifier[i] ^ expected[i]);
        done(same && difference == 0);
    });
}

void store(const QString &password, std::function<void()> done)
{
    QByteArray salt(16, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(salt.data()), 4);
    const QByteArray encodedSalt = salt.toBase64();
    Crypto::instance()->deriveVerifier(password, encodedSalt, [encodedSalt, done](const QByteArray &verifier, const QString &error) {
        if (!error.isEmpty())
            return;
        QSettings settings;
        settings.setValue(kSalt, encodedSalt);
        settings.setValue(kVerifier, verifier);
        s_unlocked = true;
        done();
    });
}

} // namespace

bool isSet()
{
    return !QSettings().value(kVerifier).toByteArray().isEmpty();
}

bool isUnlocked()
{
    return !isSet() || s_unlocked;
}

void lock()
{
    s_unlocked = false;
}

void unlock(QWidget *parent, std::function<void()> then)
{
    if (isUnlocked()) {
        then();
        return;
    }
    bool ok = false;
    const QString password = ask(parent, tr("Saved Logins"), tr("Enter your primary password to use your saved logins:"), &ok);
    if (!ok)
        return;
    const QPointer<QWidget> guard = parent;
    check(password, [guard, then](bool right) {
        if (!right) {
            QMessageBox::warning(guard, tr("Saved Logins"), tr("That is not your primary password."));
            return;
        }
        s_unlocked = true;
        then();
    });
}

void change(QWidget *parent)
{
    const QPointer<QWidget> guard = parent;
    const auto chooseNew = [guard] {
        bool ok = false;
        const QString first = ask(guard, tr("Primary Password"),
                                  tr("A new primary password (leave it empty to have none):"), &ok);
        if (!ok)
            return;
        if (first.isEmpty()) {
            QSettings settings;
            settings.remove(kSalt);
            settings.remove(kVerifier);
            QMessageBox::information(guard, tr("Primary Password"), tr("Your saved logins no longer ask for a primary password."));
            return;
        }
        if (first.size() < 8) {
            QMessageBox::warning(guard, tr("Primary Password"), tr("Use at least 8 characters."));
            return;
        }
        const QString second = ask(guard, tr("Primary Password"), tr("The same password again:"), &ok);
        if (!ok)
            return;
        if (second != first) {
            QMessageBox::warning(guard, tr("Primary Password"), tr("The two passwords are not the same."));
            return;
        }
        store(first, [guard] {
            QMessageBox::information(guard, tr("Primary Password"),
                                     tr("Your saved logins now ask for this password once each time SpacePenguin "
                                        "starts. It keeps other people at this computer out of them; the logins "
                                        "themselves stay encrypted by your system's keyring."));
        });
    };
    if (!isSet()) {
        chooseNew();
        return;
    }
    bool ok = false;
    const QString current = ask(parent, tr("Primary Password"), tr("Your current primary password:"), &ok);
    if (!ok)
        return;
    check(current, [guard, chooseNew](bool right) {
        if (!right) {
            QMessageBox::warning(guard, tr("Primary Password"), tr("That is not your primary password."));
            return;
        }
        s_unlocked = true;
        chooseNew();
    });
}

}

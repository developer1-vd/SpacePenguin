#include "backup.h"

#include "crypto.h"
#include "logins.h"
#include "primarypassword.h"
#include "profiles.h"
#include "vpn.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace spacepenguin::Backup {
namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Backup", text);
}

/** Settings that belong to this computer or protect it, never moved with a backup. */
bool staysHere(const QString &key)
{
    return key.startsWith(QLatin1String("logins/primaryPassword")) || key.startsWith(QLatin1String("window/"))
        || key.startsWith(QLatin1String("session/")) || key.startsWith(QLatin1String("classroom/"))
        || key == QLatin1String("settingsVersion");
}

} // namespace

QByteArray contents(const LoginStore *logins)
{
    QJsonArray loginArray;
    if (logins) {
        for (const Login &login : logins->logins()) {
            loginArray.append(QJsonObject{{QStringLiteral("origin"), login.origin},
                                          {QStringLiteral("username"), login.username},
                                          {QStringLiteral("password"), login.password},
                                          {QStringLiteral("created"), login.created.toString(Qt::ISODate)},
                                          {QStringLiteral("lastUsed"), login.lastUsed.toString(Qt::ISODate)}});
        }
    }
    // Settings as text, the way QSettings writes them on Linux: types survive the trip.
    QJsonObject settingsObject;
    const QSettings settings;
    for (const QString &key : settings.allKeys()) {
        if (staysHere(key))
            continue;
        QByteArray value;
        QDataStream stream(&value, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_5);
        stream << settings.value(key);
        settingsObject.insert(key, QString::fromLatin1(value.toBase64()));
    }
    return QJsonDocument(QJsonObject{{QStringLiteral("app"), QStringLiteral("SpacePenguin")},
                                     {QStringLiteral("backupVersion"), 1},
                                     {QStringLiteral("logins"), loginArray},
                                     {QStringLiteral("settings"), settingsObject}})
        .toJson(QJsonDocument::Compact);
}

bool restore(const QByteArray &data, LoginStore *logins, int *loginCount)
{
    const QJsonObject backup = QJsonDocument::fromJson(data).object();
    if (backup.value(QStringLiteral("app")).toString() != QLatin1String("SpacePenguin")
        || backup.value(QStringLiteral("backupVersion")).toInt() != 1)
        return false;
    QSettings settings;
    const QJsonObject settingsObject = backup.value(QStringLiteral("settings")).toObject();
    for (auto it = settingsObject.begin(); it != settingsObject.end(); ++it) {
        if (staysHere(it.key()))
            continue;
        QByteArray value = QByteArray::fromBase64(it.value().toString().toLatin1());
        QDataStream stream(&value, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_5);
        QVariant variant;
        stream >> variant;
        if (stream.status() == QDataStream::Ok)
            settings.setValue(it.key(), variant);
    }
    int count = 0;
    if (logins) {
        for (const QJsonValue &value : backup.value(QStringLiteral("logins")).toArray()) {
            const QJsonObject login = value.toObject();
            const QString origin = login.value(QStringLiteral("origin")).toString();
            const QString password = login.value(QStringLiteral("password")).toString();
            if (origin.isEmpty() || password.isEmpty())
                continue;
            logins->save(origin, login.value(QStringLiteral("username")).toString(), password);
            ++count;
        }
    }
    if (loginCount)
        *loginCount = count;
    return true;
}

void exportTo(QWidget *parent)
{
    const QPointer<QWidget> guard = parent;
    PrimaryPassword::unlock(parent, [guard] {
        const QString path = QFileDialog::getSaveFileName(
            guard, tr("Export Encrypted Backup"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/SpacePenguin.spbackup"),
            tr("SpacePenguin backups (*.spbackup)"));
        if (path.isEmpty())
            return;
        bool ok = false;
        const QString password = QInputDialog::getText(guard, tr("Export Encrypted Backup"),
                                                       tr("A password for the backup (at least 8 characters). "
                                                          "You need it to restore the backup; it cannot be recovered."),
                                                       QLineEdit::Password, QString(), &ok);
        if (!ok)
            return;
        if (password.size() < 8) {
            QMessageBox::warning(guard, tr("Export Encrypted Backup"), tr("Use at least 8 characters."));
            return;
        }
        const QString again = QInputDialog::getText(guard, tr("Export Encrypted Backup"), tr("The same password again:"),
                                                    QLineEdit::Password, QString(), &ok);
        if (!ok)
            return;
        if (again != password) {
            QMessageBox::warning(guard, tr("Export Encrypted Backup"), tr("The two passwords are not the same."));
            return;
        }
        QApplication::setOverrideCursor(Qt::WaitCursor);
        Crypto::instance()->encrypt(contents(sharedLogins()), password, [guard, path](const QByteArray &envelope, const QString &error) {
            QApplication::restoreOverrideCursor();
            QSaveFile file(path);
            if (!error.isEmpty() || !file.open(QIODevice::WriteOnly) || file.write(envelope) != envelope.size()
                || !file.commit()) {
                QMessageBox::warning(guard, tr("Export Encrypted Backup"), tr("The backup could not be written."));
                return;
            }
            QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            QMessageBox::information(guard, tr("Export Encrypted Backup"),
                                     tr("Your saved logins and settings are in %1, encrypted with AES-256.")
                                         .arg(QDir::toNativeSeparators(path)));
        });
    });
}

void importFrom(QWidget *parent)
{
    const QPointer<QWidget> guard = parent;
    PrimaryPassword::unlock(parent, [guard] {
        const QString path = QFileDialog::getOpenFileName(guard, tr("Restore Encrypted Backup"),
                                                          QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                          tr("SpacePenguin backups (*.spbackup)"));
        if (path.isEmpty())
            return;
        QFile file(path);
        const QByteArray envelope = file.open(QIODevice::ReadOnly) ? file.read(64 * 1024 * 1024) : QByteArray();
        if (!Crypto::isEnvelope(envelope)) {
            QMessageBox::warning(guard, tr("Restore Encrypted Backup"), tr("This is not a SpacePenguin backup."));
            return;
        }
        bool ok = false;
        const QString password = QInputDialog::getText(guard, tr("Restore Encrypted Backup"), tr("The backup's password:"),
                                                       QLineEdit::Password, QString(), &ok);
        if (!ok)
            return;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        Crypto::instance()->decrypt(envelope, password, [guard](const QByteArray &plain, const QString &error) {
            QApplication::restoreOverrideCursor();
            if (!error.isEmpty()) {
                QMessageBox::warning(guard, tr("Restore Encrypted Backup"), error);
                return;
            }
            if (QMessageBox::question(guard, tr("Restore Encrypted Backup"),
                                      tr("Restore this backup? Its settings replace yours, and its logins are "
                                         "added to yours. SpacePenguin then restarts."))
                != QMessageBox::Yes)
                return;
            int count = 0;
            if (!restore(plain, sharedLogins(), &count)) {
                QMessageBox::warning(guard, tr("Restore Encrypted Backup"), tr("This is not a SpacePenguin backup."));
                return;
            }
            Vpn::restart();
            QApplication::closeAllWindows();
        });
    });
}

}

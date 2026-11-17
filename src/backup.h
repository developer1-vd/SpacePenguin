#pragma once

#include <QByteArray>
#include <QString>

class QWidget;

namespace spacepenguin {

class LoginStore;

/**
 * An encrypted backup of what is SpacePenguin's to move to another computer:
 * the saved logins and every setting (bookmarks, search engines, roles, site
 * choices…), encrypted with a password of the person's choosing (crypto.h:
 * AES-256-GCM, PBKDF2). History and cookies stay behind.
 */
namespace Backup {

/** The backup's contents, before encryption (JSON). */
QByteArray contents(const LoginStore *logins);
/** Puts @p contents back: logins are added, settings replaced. False if it is not a backup. */
bool restore(const QByteArray &contents, LoginStore *logins, int *loginCount = nullptr);

/** Asks for a file and a password, and writes the encrypted backup. */
void exportTo(QWidget *parent);
/** Asks for a backup file and its password, and restores it. */
void importFrom(QWidget *parent);

}

}

#pragma once

#include <QString>

#include <functional>

class QWidget;

namespace spacepenguin {

/**
 * An optional primary password in front of the saved logins: once set, the
 * logins cannot be seen, filled in or exported in a session until it is given.
 *
 * Only a PBKDF2 verifier of it is kept (600,000 rounds, its own salt), never the
 * password. The logins themselves stay encrypted where they already are, in the
 * system's keyring or with the Windows account; the primary password is what
 * also protects them from someone else at an unlocked computer.
 */
namespace PrimaryPassword {

bool isSet();
bool isUnlocked();
/** Locks again, as at start-up. */
void lock();

/** Runs @p then once the logins may be used: at once, or after asking for the password. */
void unlock(QWidget *parent, std::function<void()> then);

/** Asks for a new primary password (or the current one, to change or remove it). */
void change(QWidget *parent);

}

}

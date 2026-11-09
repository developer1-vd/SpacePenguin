#pragma once

#include <QByteArray>
#include <QString>

namespace spacepenguin {

/**
 * The system's keyring: the Keychain on macOS, the Secret Service (GNOME Keyring,
 * KWallet and others) on Linux and the BSDs. It holds small secrets for the user's
 * account, encrypted at rest and unlocked with their login.
 *
 * Each secret is filed under a key of SpacePenguin's choosing. Calls are
 * synchronous; reading may wait while the keyring asks for its password.
 * Windows has none of this here: logins.cpp encrypts with DPAPI instead.
 */
namespace SecretStore {

enum class Result {
    Found,       // read: the secret is in data
    NotFound,    // read: nothing is stored under the key
    Unavailable, // no keyring, or it refused (stayed locked, an error)
};

/** Whether a keyring can be used at all. SPACEPENGUIN_NO_KEYRING=1 turns it off. */
bool isAvailable();
/** "the macOS Keychain", "your desktop's keyring"; empty when unavailable. */
QString name();

Result read(const QString &key, QByteArray *data);
bool write(const QString &key, const QByteArray &data, const QString &label);
bool remove(const QString &key);

}

}

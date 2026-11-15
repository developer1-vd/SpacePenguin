#pragma once

#include <QString>
#include <QUrl>

namespace spacepenguin {

/**
 * HTTPS-only mode: with it on, a plain http address is tried over https first,
 * and a site that has no working https gets a warning page instead of loading
 * unencrypted, from which the person can go on to it anyway. That choice lasts
 * for the site until SpacePenguin quits. Addresses on this computer or the local
 * network, and onion sites (encrypted by Tor itself), are never upgraded.
 */
namespace HttpsOnly {

/** Whether @p url would be loaded over https instead. */
bool shouldUpgrade(const QUrl &url);

/** @p url over https (the default port 80 becomes 443). */
QUrl upgraded(const QUrl &url);

/** Lets @p host load over plain http until SpacePenguin quits. */
void allow(const QString &host);
bool isAllowed(const QString &host);
/** Forgets every site let through (for tests). */
void clearAllowed();

/** Where the warning page's links point: sp://https-only/continue and …/back. */
inline const QString kContinueHost = QStringLiteral("https-only");
QUrl continueUrl();
QUrl backUrl();

/** The page shown instead of @p insecureUrl, which has no working https. */
QString warningPage(const QUrl &insecureUrl);

}

}

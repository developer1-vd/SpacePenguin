#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

class QWidget;

namespace spacepenguin {

/**
 * Private windows through Tor, which hides your address from sites and opens
 * .onion sites.
 *
 * Chromium takes one proxy for the whole process, so Tor windows run in a process
 * of their own, started with --tor: everything in it is private and goes through
 * Tor, and the rest of the browser never does. The Tor network itself comes from a
 * Tor already running on this computer (the tor service on port 9050, or Tor
 * Browser's on 9150), or from the tor program, which this process then starts
 * for as long as it runs.
 *
 * It is not Tor Browser: sites can still tell this browser from others by its
 * fingerprint. It is for reaching onion sites and keeping your address to yourself.
 */
namespace Tor {

/** Whether this process is the Tor one. */
bool isActive();
/** Whether @p argv asks for the Tor process (--tor), before Qt parses it. */
bool requested(int argc, char *argv[]);

/**
 * Finds Tor or starts it, showing its progress, and points Chromium at it. Call
 * after the QApplication exists and before the first profile does. False (and a
 * message for the person) when there is no Tor to use.
 */
bool start(QString *error);

/** Whether @p url is an onion site. */
bool isOnion(const QUrl &url);

/** Starts a Tor process with a private window showing @p urls. */
bool openWindow(const QStringList &urls, QString *error = nullptr);

/** The search to use in Tor windows: a search engine's own onion address when it has one. */
QString searchTemplate(const QString &clearnetTemplate);

/** Whether a Tor SOCKS port answers on 127.0.0.1:@p port (it says it is not an HTTP proxy). */
bool isTorPort(quint16 port);

/** How to get Tor on this system, for the message shown when there is none. */
QString installHint();

}

}

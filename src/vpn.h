#pragma once

#include <QString>

namespace spacepenguin {

/**
 * The VPN: all of the browser's traffic, pages and its own requests alike,
 * through Tor or through a VPN or proxy server the person already has.
 *
 * SpacePenguin runs no VPN servers. "Tor" uses the Tor network, free and
 * without an account, though slower, and some sites refuse it. "Your server"
 * is a SOCKS5 or HTTP proxy, which most VPN services offer (Mullvad, NordVPN,
 * Private Internet Access and others), or one of the person's own. A VPN app
 * that covers the whole computer needs nothing here.
 *
 * Chromium takes its proxy once, when it starts, so a change takes effect
 * from the next start; restart() does that, keeping the tabs.
 */
namespace Vpn {

enum class Mode { Off, Tor, Proxy };

struct Server {
    QString type; // socks5, socks4, http or https
    QString host;
    int port = 0;
};

QString modeKey(Mode mode);
Mode modeFromKey(const QString &key);

/** "socks5://host:1080", or empty if @p server is not one that can be used safely. */
QString proxyAddress(const Server &server);

/** The mode and server in the settings. */
Mode chosenMode();
Server chosenServer();

/**
 * Puts the chosen mode to use for this process: call before the first profile.
 * False with a message when the VPN cannot start (no Tor, a bad server); the
 * browser then stays off the network until the person decides.
 */
bool apply(QString *error);

/** What this process is using now: Off unless apply() succeeded. */
Mode activeMode();
/** "Tor" or "proxy.example.com:1080", for the toolbar. */
QString describeActive();

/** Starts SpacePenguin again with the same tabs, so a new mode takes effect;
 *  the caller then quits. */
void restart();
/** In a process restart() started: waits for the old one to exit. True if this is such a restart. */
bool waitForPreviousProcess();

}

}

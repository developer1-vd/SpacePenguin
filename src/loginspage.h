#pragma once

#include <QByteArray>
#include <QUrl>

class QWebEngineUrlRequestJob;

namespace spacepenguin {

class LoginStore;

/**
 * about:logins, served as sp://logins: every saved login, to search, add, edit,
 * reveal, copy and delete. The page (html/logins.html) talks to this API:
 *
 *   GET  sp://logins/api/list     the logins, passwords included
 *   POST sp://logins/api/save     {site, username, password, oldOrigin?, oldUsername?}
 *   POST sp://logins/api/remove   {origin, username}
 *   GET  sp://logins/api/never    sites where saving was turned off
 *   POST sp://logins/api/allow    {origin}: offer to save there again
 *
 * The API answers the logins page alone: a request must come from sp://logins
 * itself (its initiator), and changes must be POSTs, so no website and no link to
 * these addresses can read or change the logins.
 */
namespace LoginsPage {

void handle(QWebEngineUrlRequestJob *job, LoginStore *logins, const QByteArray &pageHtml);

/** Whether a request from @p initiator may use the API. Exposed for testing. */
bool isAllowedCaller(const QUrl &initiator);

}

}

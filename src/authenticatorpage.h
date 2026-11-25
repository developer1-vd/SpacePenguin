#pragma once

#include <QByteArray>
#include <QUrl>

class QWebEngineUrlRequestJob;

namespace spacepenguin {

class AuthenticatorStore;

/**
 * about:authenticator, served as sp://authenticator: the two-factor codes, to
 * copy, and the accounts, to add, rename, move to another app and delete. The
 * page (html/authenticator.html) talks to this API:
 *
 *   GET  sp://authenticator/api/list     {now, accounts: [{id, issuer, name, site, type,
 *                                         digits, period, code, left}]}; no keys
 *   GET  sp://authenticator/api/info     {storage}: where the keys are kept
 *   POST sp://authenticator/api/add      {uri} or {issuer, name, key}
 *   POST sp://authenticator/api/next     {id}: the next code of an HOTP account
 *   POST sp://authenticator/api/rename   {id, issuer, name}
 *   POST sp://authenticator/api/remove   {id}
 *   POST sp://authenticator/api/export   {id}: its otpauth:// link and key
 *
 * Like about:logins, it answers its own page alone (the request's initiator must
 * be sp://authenticator) and changes must be POSTs, so no website can use it.
 */
namespace AuthenticatorPage {

void handle(QWebEngineUrlRequestJob *job, AuthenticatorStore *store, const QByteArray &pageHtml);

/** Whether a request from @p initiator may use the API. Exposed for testing. */
bool isAllowedCaller(const QUrl &initiator);

}

}

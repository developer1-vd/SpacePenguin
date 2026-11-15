#include "httpsonly.h"

#include "urlresolver.h"

#include <QCoreApplication>
#include <QSet>

namespace spacepenguin::HttpsOnly {
namespace {

QSet<QString> &allowedHosts()
{
    static QSet<QString> hosts;
    return hosts;
}

QString tr(const char *text)
{
    return QCoreApplication::translate("HttpsOnly", text);
}

} // namespace

bool shouldUpgrade(const QUrl &url)
{
    if (url.scheme().compare(QLatin1String("http"), Qt::CaseInsensitive) != 0)
        return false;
    const QString host = url.host().toLower();
    if (host.isEmpty() || host.endsWith(QLatin1String(".onion")) || UrlResolver::isLocalHost(host))
        return false;
    return !isAllowed(host);
}

QUrl upgraded(const QUrl &url)
{
    QUrl secure = url;
    secure.setScheme(QStringLiteral("https"));
    if (secure.port() == 80)
        secure.setPort(-1);
    return secure;
}

void allow(const QString &host)
{
    allowedHosts().insert(host.toLower());
}

bool isAllowed(const QString &host)
{
    return allowedHosts().contains(host.toLower());
}

void clearAllowed()
{
    allowedHosts().clear();
}

QUrl continueUrl()
{
    return QUrl(QStringLiteral("sp://") + kContinueHost + QStringLiteral("/continue"));
}

QUrl backUrl()
{
    return QUrl(QStringLiteral("sp://") + kContinueHost + QStringLiteral("/back"));
}

QString warningPage(const QUrl &insecureUrl)
{
    QString host = insecureUrl.host();
    if (host.startsWith(QLatin1String("www.")))
        host.remove(0, 4);
    return QStringLiteral(
               "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
               "<meta name=\"color-scheme\" content=\"light dark\"><title>%1</title><style>"
               "body{font:16px/1.5 system-ui,sans-serif;display:grid;place-items:center;"
               "min-height:90vh;margin:0;color:#3b3f4a}"
               "@media (prefers-color-scheme: dark){body{color:#d6d9e0;background:#15171d}"
               "a.secondary{color:#d6d9e0;border-color:#4a4f5c}}"
               "main{max-width:34rem;padding:0 1.5rem}h1{font-size:1.5rem;margin:.5rem 0}"
               "p{margin:.6rem 0;opacity:.85}.icon{font-size:2.6rem}"
               ".actions{display:flex;gap:.6rem;flex-wrap:wrap;margin-top:1.4rem}"
               "a{display:inline-block;padding:.55rem 1.1rem;border-radius:999px;text-decoration:none;"
               "font-weight:600}a.primary{background:#2f5bea;color:#fff}"
               "a.secondary{border:1px solid #c9cdd6;color:#3b3f4a}</style></head><body><main>"
               "<div class=\"icon\" aria-hidden=\"true\">🔓</div><h1>%2</h1><p>%3</p><p>%4</p>"
               "<div class=\"actions\"><a class=\"primary\" href=\"%8\">%5</a>"
               "<a class=\"secondary\" href=\"%6\">%7</a></div></main></body></html>")
        .arg(tr("Not secure").toHtmlEscaped(),
             tr("%1 has no secure connection").arg(host).toHtmlEscaped(),
             tr("You turned on HTTPS-only mode, and this site does not offer HTTPS. On an "
                "unencrypted connection, anyone on your network can see and change what you "
                "send and receive, such as passwords.").toHtmlEscaped(),
             tr("If you go on, SpacePenguin loads %1 unencrypted until you quit.")
                 .arg(host).toHtmlEscaped(),
             tr("Go back").toHtmlEscaped(),
             continueUrl().toString(),
             tr("Continue to the insecure site").toHtmlEscaped(),
             backUrl().toString());
}

}

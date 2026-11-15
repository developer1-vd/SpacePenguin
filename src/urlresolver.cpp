#include "urlresolver.h"

#include <algorithm>

#include <QDir>
#include <QFileInfo>

namespace spacepenguin {
namespace {

bool isScheme(const QString &candidate)
{
    if (candidate.isEmpty())
        return false;
    if (!candidate.at(0).isLetter())
        return false;
    for (const QChar c : candidate) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('+') && c != QLatin1Char('-')
            && c != QLatin1Char('.')) {
            return false;
        }
    }
    return true;
}

bool isHostChars(const QString &text)
{
    if (text.isEmpty())
        return false;
    for (const QChar c : text) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('-') && c != QLatin1Char('_')
            && c != QLatin1Char('.')) {
            return false;
        }
    }
    return true;
}

bool isAllDigits(const QString &text)
{
    if (text.isEmpty())
        return false;
    for (const QChar c : text) {
        if (!c.isDigit())
            return false;
    }
    return true;
}

// "example.com:8080/page?q" -> "example.com:8080": the part before any path,
// query or fragment.
QString authorityOf(const QString &text)
{
    qsizetype end = text.size();
    for (const QChar stop : {QLatin1Char('/'), QLatin1Char('?'), QLatin1Char('#')}) {
        const qsizetype at = text.indexOf(stop);
        if (at >= 0 && at < end)
            end = at;
    }
    return text.left(end);
}

bool looksLikeHostAndPort(const QString &text)
{
    // Only the host and port count: "localhost:3000/app" is a host and port too.
    const QString authority = authorityOf(text);
    const int colon = authority.lastIndexOf(QLatin1Char(':'));
    if (colon <= 0)
        return false;
    if (!isAllDigits(authority.mid(colon + 1)))
        return false;
    return isHostChars(authority.left(colon));
}

// Addresses on this computer or the local network, which nearly always serve plain
// http (a development server, a router, a NAS) and have no certificate for https.
bool isLocalHost(const QString &host)
{
    const QString name = host.toLower();
    if (name == QLatin1String("localhost") || name.endsWith(QLatin1String(".localhost")))
        return true;
    if (name.startsWith(QLatin1Char('[')))
        return true; // an IPv6 address
    const QStringList parts = name.split(QLatin1Char('.'));
    if (parts.size() == 4 && std::all_of(parts.cbegin(), parts.cend(), [](const QString &part) {
            return !part.isEmpty() && part.size() <= 3 && isAllDigits(part);
        }))
        return true; // an IPv4 address
    for (const char *suffix : {".local", ".lan", ".home.arpa", ".internal", ".intranet"}) {
        if (name.endsWith(QLatin1String(suffix)))
            return true;
    }
    // A single name with no dot ("nas", "router"): a machine on this network.
    return !name.isEmpty() && !name.contains(QLatin1Char('.'));
}

} // namespace

QString UrlResolver::defaultSearchTemplate()
{
    return QStringLiteral("https://duckduckgo.com/?q={query}");
}

QString UrlResolver::defaultHomeUrl()
{
    return QStringLiteral("sp://start/");
}

UrlResolver::UrlResolver(QString searchTemplate, QString homeUrl)
    : m_searchTemplate(std::move(searchTemplate))
    , m_homeUrl(std::move(homeUrl))
{
    if (m_searchTemplate.isEmpty())
        m_searchTemplate = defaultSearchTemplate();
    if (m_homeUrl.isEmpty())
        m_homeUrl = defaultHomeUrl();
}

bool UrlResolver::looksLikeUrl(const QString &input)
{
    const QString text = input.trimmed();
    if (text.isEmpty())
        return false;

    const int colon = text.indexOf(QLatin1Char(':'));
    if (colon > 0 && isScheme(text.left(colon)))
        return true;

    if (text.contains(QLatin1Char(' ')) || text.contains(QLatin1Char('\t')))
        return false;

    if (text.startsWith(QLatin1String("//")))
        return true;

    QString authority = text;
    const int slash = text.indexOf(QLatin1Char('/'));
    if (slash != -1)
        authority = text.left(slash);

    const int portSeparator = authority.indexOf(QLatin1Char(':'));
    if (portSeparator != -1)
        authority = authority.left(portSeparator);

    if (authority.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0)
        return true;

    const int dot = authority.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0)
        return false;

    const QString host = authority.left(dot);
    const QString topLevel = authority.mid(dot + 1);
    if (!isHostChars(host) || !isHostChars(topLevel))
        return false;

    if (topLevel.size() < 2 && !isAllDigits(topLevel))
        return false;

    return true;
}

QUrl UrlResolver::searchUrl(const QString &query) const
{
    QString templateUrl = m_searchTemplate;
    templateUrl.replace(QLatin1String("{query}"),
                        QString::fromUtf8(QUrl::toPercentEncoding(query)));
    return QUrl(templateUrl);
}

QUrl UrlResolver::resolve(const QString &input) const
{
    const QString text = input.trimmed();
    if (text.isEmpty())
        return QUrl(m_homeUrl);

    if (text.compare(QLatin1String("about:"), Qt::CaseInsensitive) == 0)
        return QUrl(QStringLiteral("about:about"));

    // A file on this computer, typed or pasted as a path.
    QString path = text;
    if (path.startsWith(QLatin1String("~/")))
        path = QDir::homePath() + path.mid(1);
    if (QFileInfo(path).isAbsolute() && QFileInfo::exists(path))
        return QUrl::fromLocalFile(path);

    if (!looksLikeUrl(text))
        return searchUrl(text);

    const int colon = text.indexOf(QLatin1Char(':'));
    if (colon > 0 && !looksLikeHostAndPort(text))
        return QUrl(text);

    if (text.startsWith(QLatin1String("//")))
        return QUrl(QStringLiteral("https:") + text);

    // Onion sites are encrypted by Tor itself and mostly serve plain http, and so
    // do addresses on this computer or the local network.
    const QString host = authorityOf(text).section(QLatin1Char(':'), 0, 0);
    if (host.endsWith(QLatin1String(".onion"), Qt::CaseInsensitive) || isLocalHost(host))
        return QUrl(QStringLiteral("http://") + text);

    return QUrl(QStringLiteral("https://") + text);
}

}
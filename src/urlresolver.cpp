#include "urlresolver.h"

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

bool looksLikeHostAndPort(const QString &text)
{
    const int colon = text.lastIndexOf(QLatin1Char(':'));
    if (colon <= 0)
        return false;
    if (!isAllDigits(text.mid(colon + 1)))
        return false;
    return isHostChars(text.left(colon));
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

    if (!looksLikeUrl(text))
        return searchUrl(text);

    const int colon = text.indexOf(QLatin1Char(':'));
    if (colon > 0 && !looksLikeHostAndPort(text))
        return QUrl(text);

    if (text.startsWith(QLatin1String("//")))
        return QUrl(QStringLiteral("https:") + text);

    return QUrl(QStringLiteral("https://") + text);
}

}
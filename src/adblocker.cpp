#include "adblocker.h"

#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QWebEngineUrlRequestInfo>

namespace spacepenguin {
namespace {

bool isCommentOrHeader(const QString &line)
{
    return line.startsWith(QLatin1Char('!')) || line.startsWith(QLatin1Char('#'))
        || line.startsWith(QLatin1Char('['));
}

/**
 * uBlock Origin and Adblock Plus extensions this blocker does not run: procedural
 * and scriptlet filters, HTML filtering, CSS injection. Skipped whole, rather than
 * read as patterns that never match (or, as selectors, break the page's stylesheet).
 */
bool isUnsupportedSyntax(const QString &line)
{
    static const char *const markers[] = {"#?#", "#@?#", "#$#", "#@$#", "#%#", "#@%#", "$$", "##+js(",
                                          "#@#+js(", "##^", ":-abp-", ":has-text(", ":matches-css",
                                          ":xpath(", ":upward(", ":remove(", ":style(", ":min-text-length(",
                                          ":watch-attr(", ":matches-path(", ":others(", ":matches-attr(",
                                          ":matches-prop(", ":remove-attr(", ":remove-class("};
    for (const char *marker : markers) {
        if (line.contains(QLatin1String(marker)))
            return true;
    }
    return false;
}

inline bool isTokenChar(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9');
}

/** The words of an address, lowercase, split at everything but letters and digits. */
QStringList urlTokens(const QString &url)
{
    QStringList tokens;
    const QString lower = url.toLower();
    int start = -1;
    for (int i = 0; i <= lower.size(); ++i) {
        if (i < lower.size() && isTokenChar(lower.at(i))) {
            if (start < 0)
                start = i;
        } else if (start >= 0) {
            if (i - start >= 2)
                tokens.append(lower.mid(start, i - start));
            start = -1;
        }
    }
    tokens.removeDuplicates();
    return tokens;
}

/**
 * The words every address the pattern matches must contain whole: ones bounded on
 * both sides by a separator, an anchor or `^`, not by a wildcard or the pattern's
 * open ends. None for a regular expression.
 */
QStringList patternTokens(const QString &pattern)
{
    QStringList tokens;
    if (pattern.size() > 2 && pattern.startsWith(QLatin1Char('/')) && pattern.endsWith(QLatin1Char('/')))
        return tokens;
    const QString lower = pattern.toLower();
    int start = -1;
    for (int i = 0; i <= lower.size(); ++i) {
        if (i < lower.size() && isTokenChar(lower.at(i))) {
            if (start < 0)
                start = i;
            continue;
        }
        if (start < 0)
            continue;
        const bool leftBounded = start > 0 && lower.at(start - 1) != QLatin1Char('*');
        const bool rightBounded = i < lower.size() && lower.at(i) != QLatin1Char('*');
        if (leftBounded && rightBounded && i - start >= 2)
            tokens.append(lower.mid(start, i - start));
        start = -1;
    }
    return tokens;
}

/** "example.com" for `||example.com^`: a rule that is only a host, checked without a regex. */
QString anchoredHost(const QString &pattern)
{
    static const QRegularExpression hostOnly(QStringLiteral("^\\|\\|([a-z0-9.-]+)\\^?$"));
    const QRegularExpressionMatch match = hostOnly.match(pattern.toLower());
    return match.hasMatch() ? match.captured(1) : QString();
}

QString hostSuffix(const QString &host)
{
    return QLatin1Char('.') + host;
}

bool hostMatchesSuffix(const QString &host, const QString &suffix)
{
    return host.endsWith(suffix) && host.size() > suffix.size();
}

QString unescapePattern(QString pattern)
{
    QString result;
    result.reserve(pattern.size());
    for (int index = 0; index < pattern.size(); ++index) {
        const QChar character = pattern.at(index);
        if (character != QLatin1Char('\\')) {
            result.append(character);
            continue;
        }

        if (index + 1 >= pattern.size()) {
            result.append(character);
            break;
        }

        const QChar next = pattern.at(++index);
        if (next == QLatin1Char('u')) {
            if (index + 4 < pattern.size()) {
                bool ok = false;
                const uint code = pattern.mid(index + 1, 4).toUInt(&ok, 16);
                if (ok) {
                    const char32_t character = char32_t(code);
                    result.append(QString::fromUcs4(&character, 1));
                    index += 4;
                    continue;
                }
            }
            result.append(QLatin1Char('u'));
            continue;
        }

        result.append(next);
    }

return result;
}

QString separatorPattern()
{
    return QStringLiteral("[^a-zA-Z0-9_.%-]");
}

QString endOfAddressPattern()
{
    // A trailing `?` or `$` means the end of the address: either the end of the URL or
    // the `/`, `?` or `#` that starts a path or query. Kept deliberately simple: an
    // optional leading group holding a literal makes PCRE skip every start position.
    return QStringLiteral("(?:[/?#]|$)");
}


/**
 * Translates an Adblock Plus style filter into a regular expression source that is
 * matched against the full URL and against the URL without its scheme.
 */
QString patternToRegExpSourceInternal(const QString &filter)
{
    // `/…/` is a regular expression already.
    if (filter.size() > 2 && filter.startsWith(QLatin1Char('/')) && filter.endsWith(QLatin1Char('/')))
        return filter.mid(1, filter.size() - 2);

    QString pattern = unescapePattern(filter);
    QString source;
    bool anchorStart = false;
    bool anchorEnd = false;
    bool domainAnchored = false;

    if (pattern.startsWith(QLatin1String("||"))) {
        domainAnchored = true;
        pattern = pattern.mid(2);
    } else if (pattern.startsWith(QLatin1Char('|'))) {
        anchorStart = true;
        pattern = pattern.mid(1);
    }

    if (pattern.endsWith(QLatin1Char('|')) && !pattern.endsWith(QLatin1String("\\|"))) {
        anchorEnd = true;
        pattern.chop(1);
    }

    if (anchorStart || domainAnchored)
        source += QLatin1Char('^');

    if (domainAnchored)
        source += QLatin1String("(?:[^/?#]*\\.)?");

    int index = 0;
    const int size = pattern.size();
    while (index < size) {
        const QChar character = pattern.at(index);

        if (character == QLatin1Char('*')) {
            source += QLatin1String(".*");
            ++index;
            continue;
        }

        if (character == QLatin1Char('^')) {
            if (index + 1 < size && pattern.at(index + 1) == QLatin1Char('^')) {
                // A literal caret.
                source += separatorPattern();
                index += 2;
                continue;
            }
            source += separatorPattern();
            ++index;
            continue;
        }

        if (character == QLatin1Char('?') && index == size - 1) {
            source += endOfAddressPattern();
            ++index;
            continue;
        }

        if (character == QLatin1Char('$') && index == size - 1) {
            source += endOfAddressPattern();
            ++index;
            continue;
        }

        source += QRegularExpression::escape(QString(character));
        ++index;
    }

    if (anchorEnd)
        source += QLatin1Char('$');

    return source;
}

QRegularExpression compilePattern(const QString &filter, bool caseSensitive)
{
    QRegularExpression::PatternOptions options =
        QRegularExpression::UseUnicodePropertiesOption;
    if (!caseSensitive)
        options |= QRegularExpression::CaseInsensitiveOption;

    return QRegularExpression(patternToRegExpSourceInternal(filter), options);
}

bool knownOption(const QString &option)
{
    static const QStringList names = {
        QStringLiteral("third-party"),   QStringLiteral("3p"),
        QStringLiteral("~third-party"),  QStringLiteral("1p"),
        QStringLiteral("domain"),        QStringLiteral("from"),
        QStringLiteral("script"),        QStringLiteral("image"),
        QStringLiteral("stylesheet"),     QStringLiteral("css"),
        QStringLiteral("object"),        QStringLiteral("object-subrequest"),
        QStringLiteral("xmlhttprequest"), QStringLiteral("xhr"),
        QStringLiteral("websocket"),     QStringLiteral("media"),
        QStringLiteral("font"),          QStringLiteral("ping"),
        QStringLiteral("beacon"),        QStringLiteral("csp"),
        QStringLiteral("document"),      QStringLiteral("doc"),
        QStringLiteral("subdocument"),   QStringLiteral("subdoc"),
        QStringLiteral("popup"),         QStringLiteral("elem"),
        QStringLiteral("generichide"),   QStringLiteral("ghide"),
        QStringLiteral("elemhide"),      QStringLiteral("ehide"),
        QStringLiteral("websocket"),     QStringLiteral("other"),
        QStringLiteral("important"),     QStringLiteral("match-case"),
        QStringLiteral("removeparam"),
    };
    return names.contains(option);
}

bool isCosmeticOnlyOption(const QString &option)
{
    return option == QLatin1String("generichide") || option == QLatin1String("ghide")
        || option == QLatin1String("elemhide") || option == QLatin1String("ehide");
}

RequestType typeFromOption(const QString &option, bool *matched)
{
    struct Entry {
        const char *name;
        RequestType type;
    };

    static const Entry table[] = {
        {"script", RequestType::Script},         {"image", RequestType::Image},
        {"stylesheet", RequestType::Stylesheet}, {"css", RequestType::Stylesheet},
        {"object", RequestType::Object},         {"object-subrequest", RequestType::Object},
        {"xmlhttprequest", RequestType::Xhr},    {"xhr", RequestType::Xhr},
        {"websocket", RequestType::WebSocket},   {"media", RequestType::Media},
        {"font", RequestType::Font},             {"ping", RequestType::Ping},
        {"beacon", RequestType::Ping},           {"csp", RequestType::Other},
        {"document", RequestType::Document},     {"doc", RequestType::Document},
        {"subdocument", RequestType::SubDocument}, {"subdoc", RequestType::SubDocument},
        {"popup", RequestType::Popup},           {"elem", RequestType::Other},
        {"generichide", RequestType::Other},     {"ghide", RequestType::Other},
        {"other", RequestType::Other},
    };

    for (const Entry &entry : table) {
        if (option == QLatin1String(entry.name)) {
            *matched = true;
            return entry.type;
        }
    }

    *matched = false;
    return RequestType::Any;
}

bool domainListMatches(const QStringList &included, const QStringList &excluded,
                       const QString &documentHost)
{
    if (documentHost.isEmpty())
        return included.isEmpty();

    for (const QString &domain : excluded) {
        if (documentHost == domain || hostMatchesSuffix(documentHost, hostSuffix(domain)))
            return false;
    }

    if (included.isEmpty())
        return true;

    for (const QString &domain : included) {
        if (documentHost == domain || hostMatchesSuffix(documentHost, hostSuffix(domain)))
            return true;
    }

    return false;
}

} // namespace

QString AdBlocker::patternToRegExpSource(const QString &filter)
{
    return patternToRegExpSourceInternal(filter);
}

QStringList requestTypeNames()
{
    return {QStringLiteral("script"),  QStringLiteral("image"), QStringLiteral("stylesheet"),
            QStringLiteral("font"),    QStringLiteral("media"), QStringLiteral("object"),
            QStringLiteral("xhr"),     QStringLiteral("ping"),  QStringLiteral("websocket"),
            QStringLiteral("document"), QStringLiteral("popup")};
}

bool NetworkRule::matchesDomain(const QString &documentHost) const
{
    return domainListMatches(includedDomains, excludedDomains, documentHost.toLower());
}

bool NetworkRule::matches(const QUrl &url, const QString &documentHost,
                          RequestType requestType) const
{
    if (type != RequestType::Any && requestType != RequestType::Any && type != requestType)
        return false;

    if (!includedDomains.isEmpty() || !excludedDomains.isEmpty()) {
        if (documentHost.isEmpty())
            return false;
        if (!matchesDomain(documentHost))
            return false;
    }

    const QString host = url.host();
    if (host.isEmpty())
        return false;

    if (thirdPartyOnly) {
        if (documentHost.isEmpty())
            return false;
        const QString document = documentHost.toLower();
        if (document == host.toLower()
            || hostMatchesSuffix(host.toLower(), hostSuffix(document)))
            return false;
    }

    if (!hostOnly.isEmpty()) {
        const QString lowerHost = host.toLower();
        return lowerHost == hostOnly || hostMatchesSuffix(lowerHost, hostSuffix(hostOnly));
    }

    static thread_local QHash<QString, QRegularExpression> cache;

    const QString key = caseSensitive ? QStringLiteral("1") + pattern : pattern;

    QRegularExpression compiled;
    const auto cached = cache.constFind(key);
    if (cached != cache.constEnd()) {
        compiled = cached.value();
    } else {
        compiled = compilePattern(pattern, caseSensitive);
        if (cache.size() > 20000)
            cache.clear();
        cache.insert(key, compiled);
    }

    if (!compiled.isValid())
        return false;

    const QString full = url.toString();
    if (compiled.match(full).hasMatch())
        return true;

    const QString scheme = url.scheme();
    if (!scheme.isEmpty()) {
        QString withoutScheme = full;
        withoutScheme.remove(0, scheme.size() + 3);
        if (compiled.match(withoutScheme).hasMatch())
            return true;
    }

    const QString hostAndPath = host.toLower() + url.path().toLower();
    return compiled.match(hostAndPath).hasMatch();
}

bool CosmeticRule::matchesDomain(const QString &documentHost) const
{
    return domainListMatches(includedDomains, excludedDomains, documentHost.toLower());
}

AdBlocker::AdBlocker(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
}

bool AdBlocker::parseRule(const QString &line, NetworkRule *rule)
{
    if (line.isEmpty())
        return false;

    QString filter = line;
    const bool negated = filter.startsWith(QLatin1String("@@"));
    if (negated)
        filter = filter.mid(2);

    if (filter.isEmpty())
        return false;

    *rule = NetworkRule();
    rule->source = line;

    // Options are separated by the last unescaped `$`; an unknown option name
    // means the whole line is a plain pattern, as in Adblock Plus.
    int optionStart = -1;
    for (int index = filter.size() - 1; index >= 0; --index) {
        if (filter.at(index) != QLatin1Char('$'))
            continue;
        if (index > 0 && filter.at(index - 1) == QLatin1Char('\\')) {
            index -= 1;
            continue;
        }
        optionStart = index;
        break;
    }

    if (optionStart >= 0) {
        const QString optionText = filter.mid(optionStart + 1).toLower();
        const QStringList options = optionText.split(QLatin1Char(','));

        QStringList domains;
        QStringList excludedDomains;
        bool thirdParty = false;
        bool thirdPartyExcluded = false;
        bool caseSensitive = false;
        bool cosmeticOnly = false;
        bool ok = true;

        for (const QString &rawOption : options) {
            const QString option = rawOption.trimmed();
            if (option.isEmpty()) {
                ok = false;
                break;
            }

            const int equals = option.indexOf(QLatin1Char('='));
            const QString name = equals < 0 ? option : option.left(equals);
            const QString value = equals < 0 ? QString() : option.mid(equals + 1);

            if (!knownOption(name)) {
                // An option this blocker does not support ($redirect, $removeparam, …):
                // the rule cannot be honoured as written, so it is left out.
                return false;
            }

            if (isCosmeticOnlyOption(name)) {
                cosmeticOnly = true;
                continue;
            }

            if (name == QLatin1String("removeparam")) {
                rule->isRemoveParam = true;
                // The value as written: a regex is case-sensitive.
                if (equals >= 0) {
                    const QString written = filter.mid(optionStart + 1);
                    const int start = written.indexOf(QLatin1Char('='),
                                                      written.indexOf(QLatin1String("removeparam"), 0, Qt::CaseInsensitive)) + 1;
                    const int end = written.indexOf(QLatin1Char(','), start);
                    rule->removeParam = written.mid(start, end < 0 ? -1 : end - start);
                }
                continue;
            }

            if (name == QLatin1String("match-case")) {
                caseSensitive = true;
                continue;
            }

            if (name == QLatin1String("third-party") || name == QLatin1String("3p")) {
                thirdParty = true;
                continue;
            }
            if (name == QLatin1String("~third-party") || name == QLatin1String("1p")) {
                thirdPartyExcluded = true;
                continue;
            }
            if (name == QLatin1String("domain") || name == QLatin1String("from")) {
                if (value.isEmpty()) {
                    ok = false;
                    break;
                }
                for (const QString &domain : value.split(QLatin1Char('|'))) {
                    const QString trimmed = domain.trimmed().toLower();
                    if (trimmed.startsWith(QLatin1Char('~')))
                        excludedDomains.append(trimmed.mid(1));
                    else
                        domains.append(trimmed);
                }
                continue;
            }

            bool matched = false;
            const RequestType type = typeFromOption(name, &matched);
            if (matched) {
                if (type == RequestType::Other)
                    continue;
                if (rule->type != RequestType::Any && rule->type != type) {
                    ok = false;
                    break;
                }
                rule->type = type;
            }
        }

        if (ok) {
            filter = filter.left(optionStart);
            rule->includedDomains = domains;
            rule->excludedDomains = excludedDomains;
            rule->thirdPartyOnly = thirdParty;
            rule->caseSensitive = caseSensitive;

            if (cosmeticOnly) {
                // `$generichide` and friends only affect element hiding, never the
                // network. Treating them as network rules would let a list cancel its
                // own blocking rules.
                return false;
            }
        } else if (thirdPartyExcluded) {
            // `$~third-party` cannot widen a pattern; keep the rule but ignore it.
            rule->thirdPartyOnly = false;
        }
    }

    // `$removeparam=…` on its own applies to every address.
    if (filter.isEmpty() && rule->isRemoveParam)
        filter = QStringLiteral("*");
    if (filter.isEmpty())
        return false;

    // Only a regular-expression rule can fail to compile; the rest are escaped.
    if (filter.size() > 2 && filter.startsWith(QLatin1Char('/')) && filter.endsWith(QLatin1Char('/'))
        && !compilePattern(filter, false).isValid()) {
        return false;
    }

    rule->pattern = filter;
    rule->hostOnly = anchoredHost(filter);
    return true;
}

bool AdBlocker::parseCosmeticRule(const QString &line, CosmeticRule *rule)
{
    const int marker = line.indexOf(QLatin1String("##"));
    const int exceptionMarker = line.indexOf(QLatin1String("#@#"));

    int domainsEnd = -1;
    bool negated = false;
    QString selector;

    if (exceptionMarker >= 0 && (marker < 0 || exceptionMarker < marker)) {
        domainsEnd = exceptionMarker;
        negated = true;
        selector = line.mid(exceptionMarker + 3);
    } else if (marker >= 0) {
        domainsEnd = marker;
        selector = line.mid(marker + 2);
    } else {
        return false;
    }

    if (selector.isEmpty())
        return false;

    *rule = CosmeticRule();
    rule->selector = selector.trimmed();

    const QString domainPart = line.left(domainsEnd);
    for (const QString &rawDomain : domainPart.split(QLatin1Char(','))) {
        const QString domain = rawDomain.trimmed().toLower();
        if (domain.isEmpty() || domain == QLatin1String("~"))
            continue;
        if (domain.startsWith(QLatin1Char('~')))
            rule->excludedDomains.append(domain.mid(1));
        else
            rule->includedDomains.append(domain);
    }

    Q_UNUSED(negated)
    return !rule->selector.isEmpty();
}

bool AdBlocker::loadFilters(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastListError = tr("Cannot read %1").arg(path);
        if (error)
            *error = m_lastListError;
        return false;
    }

    QTextStream stream(&file);
    const QString text = stream.readAll();
    file.close();

    if (!loadFiltersFromText(text)) {
        if (error)
            *error = m_lastListError;
        return false;
    }

    m_listPath = QFileInfo(path).absoluteFilePath();
    m_lastListError.clear();
    return true;
}

std::shared_ptr<const AdBlocker::Filters> AdBlocker::parseFilters(const QString &text)
{
    auto filters = std::make_shared<Filters>();
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        if (isUnsupportedSyntax(line))
            continue;

        // Cosmetic rules are checked first: they start with `#`, but are rules.
        if (line.contains(QLatin1String("##")) || line.contains(QLatin1String("#@#"))) {
            CosmeticRule cosmetic;
            if (parseCosmeticRule(line, &cosmetic)) {
                if (line.contains(QLatin1String("#@#")))
                    filters->cosmeticExceptions.append(cosmetic);
                else
                    filters->cosmeticRules.append(cosmetic);
            }
            continue;
        }

        if (isCommentOrHeader(line))
            continue;

        NetworkRule rule;
        if (!parseRule(line, &rule))
            continue;

        if (rule.isRemoveParam) {
            // Exceptions to them are not supported; the rule itself is kept.
            if (!line.startsWith(QLatin1String("@@")))
                filters->removeParams.append(rule);
        } else if (line.startsWith(QLatin1String("@@"))) {
            filters->exceptions.append(rule);
        } else {
            filters->rules.append(rule);
        }
    }
    filters->ruleIndex = buildIndex(filters->rules);
    filters->exceptionIndex = buildIndex(filters->exceptions);
    return filters;
}

void AdBlocker::setFilters(std::shared_ptr<const Filters> filters)
{
    std::atomic_store(&m_filters, std::move(filters));
    emit filtersChanged();
}

bool AdBlocker::loadFiltersFromText(const QString &text)
{
    std::shared_ptr<const Filters> filters = parseFilters(text);
    if (filters->rules.isEmpty() && filters->exceptions.isEmpty() && filters->cosmeticRules.isEmpty()
        && filters->cosmeticExceptions.isEmpty() && filters->removeParams.isEmpty()) {
        m_lastListError = tr("The filter list contains no usable rules.");
        return false;
    }
    setFilters(std::move(filters));
    return true;
}

void AdBlocker::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (!m_enabled)
        return;

    const QUrl url = info.requestUrl();
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("sp") || scheme == QLatin1String("about")
        || scheme == QLatin1String("data") || scheme == QLatin1String("qrc")) {
        return;
    }

    const QString host = url.host();
    if (isPausedFor(host))
        return;

    const RequestType type = static_cast<RequestType>(static_cast<int>(info.resourceType()));
    const QString documentHost = info.initiator().host();

    if (!wouldBlock(url, type, documentHost)) {
        if (m_stripTracking && url.hasQuery()) {
            const QUrl clean = cleanUrl(url, type, documentHost);
            if (clean != url)
                info.redirect(clean);
        }
        return;
    }

    info.block(true);
    ++m_blockedCount;
    m_lastBlockedHost = host;
    emit blockedCountChanged(m_blockedCount);
}

bool AdBlocker::wouldBlock(const QUrl &url, RequestType type, const QString &documentHost) const
{
    const std::shared_ptr<const Filters> rules = filters();
    if (rules->rules.isEmpty())
        return false;

    const QString host = url.host();
    if (host.isEmpty())
        return false;

    if (isPausedFor(host))
        return false;

    const QString domain = documentHost.toLower();
    const QStringList tokens = urlTokens(url.toString());
    if (anyMatches(rules->exceptions, rules->exceptionIndex, url, tokens, domain, type))
        return false;
    return anyMatches(rules->rules, rules->ruleIndex, url, tokens, domain, type);
}

QUrl AdBlocker::cleanUrl(const QUrl &url, RequestType type, const QString &documentHost) const
{
    if (!url.hasQuery())
        return url;
    // Parameters that only say where a click came from, removed on every site.
    static const QStringList tracking = {
        QStringLiteral("utm_source"), QStringLiteral("utm_medium"), QStringLiteral("utm_campaign"),
        QStringLiteral("utm_term"),   QStringLiteral("utm_content"), QStringLiteral("utm_id"),
        QStringLiteral("fbclid"),     QStringLiteral("gclid"),       QStringLiteral("dclid"),
        QStringLiteral("gbraid"),     QStringLiteral("wbraid"),      QStringLiteral("msclkid"),
        QStringLiteral("yclid"),      QStringLiteral("twclid"),      QStringLiteral("igshid"),
        QStringLiteral("mc_eid"),     QStringLiteral("_hsenc"),      QStringLiteral("_hsmi"),
        QStringLiteral("mkt_tok"),    QStringLiteral("oly_enc_id"),  QStringLiteral("oly_anon_id"),
    };
    QList<const NetworkRule *> rules;
    const std::shared_ptr<const Filters> current = filters();
    const QString domain = documentHost.toLower();
    for (const NetworkRule &rule : current->removeParams) {
        if (rule.matches(url, domain, type))
            rules.append(&rule);
    }

    QUrlQuery query(url);
    const QList<QPair<QString, QString>> items = query.queryItems(QUrl::FullyEncoded);
    QList<QPair<QString, QString>> kept;
    for (const auto &item : items) {
        const QString name = QUrl::fromPercentEncoding(item.first.toUtf8());
        bool remove = tracking.contains(name.toLower());
        for (const NetworkRule *rule : std::as_const(rules)) {
            if (remove)
                break;
            const QString &param = rule->removeParam;
            if (param.isEmpty()) {
                remove = true;
            } else if (param.size() > 2 && param.startsWith(QLatin1Char('/')) && param.endsWith(QLatin1Char('/'))) {
                remove = QRegularExpression(param.mid(1, param.size() - 2))
                             .match(name + QLatin1Char('=') + item.second).hasMatch();
            } else {
                remove = name.compare(param, Qt::CaseInsensitive) == 0;
            }
        }
        if (!remove)
            kept.append(item);
    }
    if (kept.size() == items.size())
        return url;
    QUrl clean = url;
    QUrlQuery cleanQuery;
    cleanQuery.setQueryItems(kept);
    clean.setQuery(kept.isEmpty() ? QString() : cleanQuery.query(QUrl::FullyEncoded), QUrl::StrictMode);
    if (kept.isEmpty())
        clean.setQuery(QString());
    return clean;
}

AdBlocker::Index AdBlocker::buildIndex(const QList<NetworkRule> &rules)
{
    // Each rule goes under its rarest word, so no one word gathers thousands of rules.
    QList<QStringList> tokens;
    tokens.reserve(rules.size());
    QHash<QString, int> frequency;
    for (const NetworkRule &rule : rules) {
        tokens.append(patternTokens(rule.pattern));
        for (const QString &token : std::as_const(tokens.last()))
            ++frequency[token];
    }
    Index index;
    for (int i = 0; i < rules.size(); ++i) {
        const QStringList &candidates = tokens.at(i);
        if (candidates.isEmpty()) {
            index.unindexed.append(i);
            continue;
        }
        const QString *best = &candidates.first();
        for (const QString &token : candidates) {
            if (frequency.value(token) < frequency.value(*best))
                best = &token;
        }
        index.byToken[*best].append(i);
    }
    return index;
}

bool AdBlocker::anyMatches(const QList<NetworkRule> &rules, const Index &index, const QUrl &url,
                           const QStringList &tokens, const QString &documentHost, RequestType type)
{
    for (const QString &token : tokens) {
        const auto found = index.byToken.constFind(token);
        if (found == index.byToken.constEnd())
            continue;
        for (const int i : found.value()) {
            if (rules.at(i).matches(url, documentHost, type))
                return true;
        }
    }
    for (const int i : index.unindexed) {
        if (rules.at(i).matches(url, documentHost, type))
            return true;
    }
    return false;
}

QStringList AdBlocker::cosmeticSelectors(const QString &documentHost) const
{
    QStringList selectors;
    if (!m_enabled)
        return selectors;

    QStringList blocked;
    const auto collect = [&documentHost](const QList<CosmeticRule> &rules,
                                         QStringList *target) {
        for (const CosmeticRule &rule : rules) {
            if (!rule.matchesDomain(documentHost))
                continue;
            if (!target->contains(rule.selector))
                target->append(rule.selector);
        }
    };

    const std::shared_ptr<const Filters> rules = filters();
    collect(rules->cosmeticExceptions, &blocked);
    collect(rules->cosmeticRules, &selectors);

    for (const QString &selector : blocked)
        selectors.removeAll(selector);

    return selectors;
}

QList<CosmeticGroup> AdBlocker::cosmeticGroups() const
{
    QList<CosmeticGroup> groups;
    if (!m_enabled)
        return groups;

    // Grouped through a hash: a full list has tens of thousands of these rules.
    QHash<QString, int> groupOf;
    QList<QSet<QString>> seen;
    const auto append = [&groups, &groupOf, &seen](const QList<CosmeticRule> &rules, bool isException) {
        for (const CosmeticRule &rule : rules) {
            const QString key = rule.includedDomains.join(QLatin1Char(',')) + QLatin1Char('|')
                + rule.excludedDomains.join(QLatin1Char(',')) + (isException ? QLatin1String("|e")
                                                                             : QLatin1String("|h"));
            auto found = groupOf.constFind(key);
            if (found == groupOf.constEnd()) {
                CosmeticGroup group;
                group.domains = rule.includedDomains;
                group.excludedDomains = rule.excludedDomains;
                group.isException = isException;
                found = groupOf.insert(key, int(groups.size()));
                groups.append(group);
                seen.append(QSet<QString>());
            }
            if (!seen[*found].contains(rule.selector)) {
                seen[*found].insert(rule.selector);
                groups[*found].selectors.append(rule.selector);
            }
        }
    };

    const std::shared_ptr<const Filters> rules = filters();
    append(rules->cosmeticRules, false);
    append(rules->cosmeticExceptions, true);

    return groups;
}

void AdBlocker::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged(enabled);
}

void AdBlocker::setPausedHosts(const QStringList &hosts)
{
    m_pausedHosts.clear();
    for (const QString &host : hosts) {
        const QString trimmed = host.trimmed().toLower();
        if (!trimmed.isEmpty())
            m_pausedHosts.append(trimmed);
    }
    emit pausedHostsChanged();
}

bool AdBlocker::isPausedFor(const QString &host) const
{
    const QString target = host.trimmed().toLower();
    if (target.isEmpty())
        return false;

    for (const QString &paused : m_pausedHosts) {
        if (target == paused || hostMatchesSuffix(target, hostSuffix(paused)))
            return true;
    }

    return false;
}

}
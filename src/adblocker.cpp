#include "adblocker.h"

#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <QWebEngineUrlRequestInfo>

namespace spacepenguin {
namespace {

bool isCommentOrHeader(const QString &line)
{
    return line.startsWith(QLatin1Char('!')) || line.startsWith(QLatin1Char('#'))
        || line.startsWith(QLatin1Char('['));
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

    if (optionStart > 0) {
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
                ok = false;
                break;
            }

            if (isCosmeticOnlyOption(name)) {
                cosmeticOnly = true;
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

    if (filter.isEmpty())
        return false;

    if (!compilePattern(filter, false).isValid())
        return false;

    rule->pattern = filter;
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

bool AdBlocker::loadFiltersFromText(const QString &text)
{
    QList<NetworkRule> rules;
    QList<NetworkRule> exceptions;
    QList<CosmeticRule> cosmeticRules;
    QList<CosmeticRule> cosmeticExceptions;

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        // Cosmetic rules are checked first: they start with `#`, but are rules.
        if (line.contains(QLatin1String("##")) || line.contains(QLatin1String("#@#"))) {
            CosmeticRule cosmetic;
            if (parseCosmeticRule(line, &cosmetic)) {
                const bool negated = line.contains(QLatin1String("#@#"));
                if (negated)
                    cosmeticExceptions.append(cosmetic);
                else
                    cosmeticRules.append(cosmetic);
            }
            continue;
        }

        if (isCommentOrHeader(line))
            continue;

        NetworkRule rule;
        if (!parseRule(line, &rule))
            continue;

        if (line.startsWith(QLatin1String("@@")))
            exceptions.append(rule);
        else
            rules.append(rule);
    }

    if (rules.isEmpty() && exceptions.isEmpty() && cosmeticRules.isEmpty()
        && cosmeticExceptions.isEmpty()) {
        m_lastListError = tr("The filter list contains no usable rules.");
        return false;
    }

    m_rules = rules;
    m_exceptions = exceptions;
    m_cosmeticRules = cosmeticRules;
    m_cosmeticExceptions = cosmeticExceptions;
    emit filtersChanged();
    return true;
}

void AdBlocker::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (!m_enabled || m_rules.isEmpty())
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

    if (!wouldBlock(url, type, documentHost))
        return;

    info.block(true);
    ++m_blockedCount;
    m_lastBlockedHost = host;
    emit blockedCountChanged(m_blockedCount);
}

bool AdBlocker::wouldBlock(const QUrl &url, RequestType type, const QString &documentHost) const
{
    if (m_rules.isEmpty())
        return false;

    const QString host = url.host();
    if (host.isEmpty())
        return false;

    if (isPausedFor(host))
        return false;

    const QString domain = documentHost.toLower();

    for (const NetworkRule &rule : m_exceptions) {
        if (rule.matches(url, domain, type))
            return false;
    }

    for (const NetworkRule &rule : m_rules) {
        if (rule.matches(url, domain, type))
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

    collect(m_cosmeticExceptions, &blocked);
    collect(m_cosmeticRules, &selectors);

    for (const QString &selector : blocked)
        selectors.removeAll(selector);

    return selectors;
}

QList<CosmeticGroup> AdBlocker::cosmeticGroups() const
{
    QList<CosmeticGroup> groups;
    if (!m_enabled)
        return groups;

    const auto append = [&groups](const QList<CosmeticRule> &rules, bool isException) {
        for (const CosmeticRule &rule : rules) {
            auto it = std::find_if(groups.begin(), groups.end(), [&](const CosmeticGroup &group) {
                return group.domains == rule.includedDomains
                    && group.excludedDomains == rule.excludedDomains
                    && group.isException == isException;
            });

            if (it == groups.end()) {
                CosmeticGroup group;
                group.domains = rule.includedDomains;
                group.excludedDomains = rule.excludedDomains;
                group.isException = isException;
                group.selectors.append(rule.selector);
                groups.append(group);
                continue;
            }

            if (!it->selectors.contains(rule.selector))
                it->selectors.append(rule.selector);
        }
    };

    append(m_cosmeticRules, false);
    append(m_cosmeticExceptions, true);

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
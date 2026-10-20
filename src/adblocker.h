#pragma once

#include "cosmeticfilters.h"

#include <QHash>

#include <memory>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <QWebEngineUrlRequestInterceptor>

namespace spacepenguin {

enum class RequestType {
    Any = 0,
    Document,
    SubDocument,
    Script,
    Image,
    Stylesheet,
    Font,
    Media,
    Object,
    Xhr,
    Ping,
    WebSocket,
    Popup,
    Other,
};

QStringList requestTypeNames();

struct NetworkRule {
    QString source;
    QString pattern;
    QStringList includedDomains;
    QStringList excludedDomains;
    bool thirdPartyOnly = false;
    RequestType type = RequestType::Any;
    bool caseSensitive = false;
    /** For a rule that is only `||host^`: the host, matched without a regex. */
    QString hostOnly;

    bool matches(const QUrl &url, const QString &documentHost, RequestType requestType) const;
    bool matchesDomain(const QString &documentHost) const;
};

struct CosmeticRule {
    QString selector;
    QStringList includedDomains;
    QStringList excludedDomains;

    bool matchesDomain(const QString &documentHost) const;
};

class AdBlocker : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

public:
    explicit AdBlocker(QObject *parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

    bool loadFilters(const QString &path, QString *error = nullptr);
    bool loadFiltersFromText(const QString &text);

    /**
     * Rules by a word their pattern must contain whole, so a request is checked
     * against the few rules that could match it rather than all of them.
     */
    struct Index {
        QHash<QString, QList<int>> byToken;
        QList<int> unindexed;
    };
    /** A parsed filter list, ready to use. */
    struct Filters {
        QList<NetworkRule> rules;
        QList<NetworkRule> exceptions;
        QList<CosmeticRule> cosmeticRules;
        QList<CosmeticRule> cosmeticExceptions;
        Index ruleIndex;
        Index exceptionIndex;
    };
    /** Parses rules; safe on any thread, so a large list need not hold up the window. */
    static std::shared_ptr<const Filters> parseFilters(const QString &text);
    /** Puts parsed rules to use. Requests being checked meanwhile finish with the old ones. */
    void setFilters(std::shared_ptr<const Filters> filters);
    void setListPath(const QString &path) { m_listPath = path; }

    int ruleCount() const { return filters()->rules.size(); }
    int exceptionCount() const { return filters()->exceptions.size(); }
    int cosmeticRuleCount() const { return filters()->cosmeticRules.size(); }
    int cosmeticExceptionCount() const { return filters()->cosmeticExceptions.size(); }

    QString listPath() const { return m_listPath; }
    QString lastListError() const { return m_lastListError; }

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void setPausedHosts(const QStringList &hosts);
    QStringList pausedHosts() const { return m_pausedHosts; }
    bool isPausedFor(const QString &host) const;

    int blockedCount() const { return m_blockedCount; }
    QString lastBlockedHost() const { return m_lastBlockedHost; }

    bool wouldBlock(const QUrl &url, RequestType type = RequestType::Any,
                    const QString &documentHost = QString()) const;
    QStringList cosmeticSelectors(const QString &documentHost) const;

    /**
     * Cosmetic rules grouped by the domains they apply to, ready to be turned into the
     * injected element-hiding script.
     */
    QList<CosmeticGroup> cosmeticGroups() const;

    static bool parseRule(const QString &line, NetworkRule *rule);
    static bool parseCosmeticRule(const QString &line, CosmeticRule *rule);
    static QString patternToRegExpSource(const QString &filter);

signals:
    void blockedCountChanged(int count);
    void enabledChanged(bool enabled);
    void filtersChanged();
    void pausedHostsChanged();

private:
    static Index buildIndex(const QList<NetworkRule> &rules);
    static bool anyMatches(const QList<NetworkRule> &rules, const Index &index, const QUrl &url,
                           const QStringList &tokens, const QString &documentHost, RequestType type);
    /** The rules in use. Requests are checked on Qt's network thread, so they are swapped whole. */
    std::shared_ptr<const Filters> filters() const { return std::atomic_load(&m_filters); }
    std::shared_ptr<const Filters> m_filters = std::make_shared<const Filters>();
    QString m_listPath;
    QString m_lastListError;
    QStringList m_pausedHosts;
    QString m_lastBlockedHost;
    int m_blockedCount = 0;
    bool m_enabled = true;
};

}
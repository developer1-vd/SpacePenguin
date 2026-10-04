#pragma once

#include "cosmeticfilters.h"

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

    int ruleCount() const { return m_rules.size(); }
    int exceptionCount() const { return m_exceptions.size(); }
    int cosmeticRuleCount() const { return m_cosmeticRules.size(); }
    int cosmeticExceptionCount() const { return m_cosmeticExceptions.size(); }

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

private:
    QList<NetworkRule> m_rules;
    QList<NetworkRule> m_exceptions;
    QList<CosmeticRule> m_cosmeticRules;
    QList<CosmeticRule> m_cosmeticExceptions;
    QString m_listPath;
    QString m_lastListError;
    QStringList m_pausedHosts;
    QString m_lastBlockedHost;
    int m_blockedCount = 0;
    bool m_enabled = true;
};

}
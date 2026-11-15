#pragma once

#include <QString>
#include <QUrl>

namespace spacepenguin {

class UrlResolver
{
public:
    explicit UrlResolver(QString searchTemplate = defaultSearchTemplate(),
                          QString homeUrl = defaultHomeUrl());

    static bool looksLikeUrl(const QString &input);
    /** On this computer or the local network: localhost, an IP address, .local,
     *  .lan and the like, or a single name such as "nas". */
    static bool isLocalHost(const QString &host);

    QUrl resolve(const QString &input) const;
    QUrl searchUrl(const QString &query) const;
    QUrl homeUrl() const { return QUrl(m_homeUrl); }

    QString searchTemplate() const { return m_searchTemplate; }
    QString homeUrlString() const { return m_homeUrl; }

    static QString defaultSearchTemplate();
    static QString defaultHomeUrl();

private:
    QString m_searchTemplate;
    QString m_homeUrl;
};

}
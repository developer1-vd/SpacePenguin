#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QObject>

class QNetworkAccessManager;
class QWebEngineUrlRequestJob;

namespace spacepenguin {

class UserScripts;

/**
 * The SpacePenguin Store at sp://store: extensions (user scripts) from Greasy Fork
 * and OpenUserJS in one search, and colour themes for the browser.
 *
 * The page asks this class, not the two sites, so it needs no access to them:
 *   sp://store/api/search?q=…   both sites' results, most installed first
 *   sp://store/api/installed    the names of the scripts already added
 *   sp://store/api/themes       the colour themes, and which one is in use
 *   sp://store/api/theme?id=…   switches to a theme
 * Only sp:// pages can reach these; websites cannot.
 *
 * Adding a script is a link to its code: the window catches it and asks first,
 * naming the sites the script can change (see BrowserPage::userScriptRequested).
 */
class Store : public QObject
{
    Q_OBJECT

public:
    explicit Store(UserScripts *scripts, QObject *parent = nullptr);

    void handle(QWebEngineUrlRequestJob *job);

    /** Greasy Fork's scripts.json, as the page's result entries. Exposed for testing. */
    static QJsonArray parseGreasyFork(const QByteArray &json);
    /** OpenUserJS's search page, as the page's result entries. Exposed for testing. */
    static QJsonArray parseOpenUserJs(const QByteArray &html);

private:
    void search(QWebEngineUrlRequestJob *job, const QString &query);
    void reply(QWebEngineUrlRequestJob *job, const QByteArray &type, const QByteArray &data);

    UserScripts *m_scripts;
    QNetworkAccessManager *m_network = nullptr;
};

}

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QWebEngineUrlSchemeHandler>

#include <functional>

namespace spacepenguin {

struct StartPageTile {
    QString title;
    QString url;
};

/**
 * Serves html/start.html over sp://. The page is static except for one JSON blob —
 * the search address, the most visited sites and whether the window is private —
 * that is filled in for each request.
 */
class StartPageSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT

public:
    using TileProvider = std::function<QList<StartPageTile>()>;

    explicit StartPageSchemeHandler(bool isPrivate, QObject *parent = nullptr);
    ~StartPageSchemeHandler() override;

    void setTileProvider(TileProvider provider) { m_tiles = std::move(provider); }

    void requestStarted(QWebEngineUrlRequestJob *job) override;

    /** The page with its data placeholder filled in. Exposed for testing. */
    static QByteArray renderPage(const QByteArray &templateHtml, const QString &searchTemplate,
                                 const QList<StartPageTile> &tiles, bool isPrivate);

private:
    QByteArray m_template;
    TileProvider m_tiles;
    bool m_isPrivate = false;
};

}

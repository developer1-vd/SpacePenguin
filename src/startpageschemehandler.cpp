#include "startpageschemehandler.h"

#include "preferences.h"
#include "shortcuts.h"
#include "theme.h"

#include <QBuffer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QWebEngineUrlRequestJob>

namespace spacepenguin {
namespace {

const QByteArray kPlaceholder = QByteArrayLiteral("/*SP_DATA*/null");

} // namespace

StartPageSchemeHandler::StartPageSchemeHandler(bool isPrivate, QObject *parent)
    : QWebEngineUrlSchemeHandler(parent)
    , m_isPrivate(isPrivate)
{
    QFile file(QStringLiteral(":/html/start.html"));
    if (file.open(QIODevice::ReadOnly))
        m_template = file.readAll();
}

StartPageSchemeHandler::~StartPageSchemeHandler() = default;

QByteArray StartPageSchemeHandler::renderPage(const QByteArray &templateHtml,
                                              const QString &searchTemplate,
                                              const QList<StartPageTile> &tiles, bool isPrivate)
{
    QJsonArray tileArray;
    for (const StartPageTile &tile : tiles) {
        const QUrl url(tile.url);
        const QString scheme = url.scheme();
        if (scheme != QLatin1String("https") && scheme != QLatin1String("http"))
            continue;
        tileArray.append(QJsonObject{{QStringLiteral("title"), tile.title},
                                     {QStringLiteral("url"), tile.url}});
    }

    // The footer shows this platform's bindings, which differ on macOS.
    const QList<std::pair<QString, Shortcuts::Command>> footer = {
        {QStringLiteral("New tab"), Shortcuts::NewTab},
        {QStringLiteral("Address bar"), Shortcuts::FocusAddressBar},
        {QStringLiteral("Bookmark"), Shortcuts::BookmarkPage},
        {QStringLiteral("History"), Shortcuts::History},
        {QStringLiteral("Downloads"), Shortcuts::Downloads},
        {QStringLiteral("Private window"), Shortcuts::NewPrivateWindow},
        {QStringLiteral("All shortcuts"), Shortcuts::KeyboardShortcuts},
    };
    QJsonArray shortcutArray;
    for (const auto &[label, command] : footer) {
        shortcutArray.append(QJsonObject{{QStringLiteral("label"), label},
                                         {QStringLiteral("keys"), Shortcuts::text(command)}});
    }

    const QJsonObject data{
        {QStringLiteral("search"), searchTemplate},
        {QStringLiteral("tiles"), tileArray},
        {QStringLiteral("private"), isPrivate},
        {QStringLiteral("shortcuts"), shortcutArray},
    };

    // '<' never appears unescaped, so page titles cannot close the <script> element.
    QByteArray json = QJsonDocument(data).toJson(QJsonDocument::Compact);
    json.replace('<', "\\u003c");
    json.replace('>', "\\u003e");

    QByteArray page = templateHtml;
    page.replace(kPlaceholder, json);
    return page;
}

void StartPageSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    if (m_template.isEmpty()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    const QList<StartPageTile> tiles = (m_tiles && !m_isPrivate) ? m_tiles()
                                                                : QList<StartPageTile>();
    QByteArray page = renderPage(m_template, Preferences::instance()->searchTemplate(), tiles,
                                 m_isPrivate);
    // Follow the browser's theme rather than only the system's.
    if (Theme::storedMode() != Theme::Mode::System) {
        page.replace(QByteArrayLiteral("<html lang=\"en\">"),
                     Theme::isDark() ? QByteArrayLiteral("<html lang=\"en\" class=\"dark\">")
                                     : QByteArrayLiteral("<html lang=\"en\" class=\"light\">"));
    }

    // One buffer per request: concurrent loads must not share a read position.
    auto *buffer = new QBuffer(job);
    buffer->setData(page);
    buffer->open(QIODevice::ReadOnly);
    job->reply(QByteArrayLiteral("text/html; charset=utf-8"), buffer);
}

}

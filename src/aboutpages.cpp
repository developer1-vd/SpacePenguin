#include "aboutpages.h"

#include <QCoreApplication>
#include <QStringList>
#include <QtGlobal>

#ifndef SPACEPENGUIN_BUILD_TYPE
#    define SPACEPENGUIN_BUILD_TYPE "unknown"
#endif

namespace spacepenguin {
namespace {

struct PageDefinition {
    const char *id;
    const char *title;
    const char *summary;
    bool easterEgg;
};

const PageDefinition kPages[] = {
    { "about:about", "About SpacePenguin",
      "Index of every internal page this browser answers to.", false },
    { "about:version", "Version",
      "Build, Qt version and profile details.", false },
    { "about:license", "Licenses",
      "What this browser is built on and under what terms.", false },
    { "about:penguin", "Penguin",
      "A penguin. Obviously.", true },
    { "about:teapot", "I'm a teapot",
      "RFC 2324, rendered with dignity.", true },
    { "about:pan", "Panned",
      "The other half of the bread.", true },
};

QString escape(const QString &text)
{
    return text.toHtmlEscaped();
}

QString buildRow(const AboutPage &page)
{
    return QStringLiteral("<tr><td><a href=\"%1\">%1</a></td><td>%2</td>"
                          "<td>%3</td></tr>")
        .arg(escape(page.id), escape(page.summary),
             page.easterEgg ? QStringLiteral("easter egg") : QString());
}

QString aboutAboutBody()
{
    QString rows;
    for (const AboutPage &page : AboutPages::all()) {
        if (page.id == QLatin1String("about:about"))
            continue;
        rows += buildRow(page);
    }

    return QStringLiteral(
               "<h1>SpacePenguin</h1>"
               "<p class=\"lead\">A secure, lightweight, and usable browser written in Qt. "
               "Everything below lives inside the browser; nothing is fetched from the network."
               "</p>"
               "<h2>Internal pages</h2>"
               "<table><thead><tr><th>Page</th><th>What it does</th><th></th></tr></thead>"
               "<tbody>%1</tbody></table>"
               "<p>Unknown internal pages answer with a short note instead of a network error. "
               "<code>about:blank</code> is handled by the rendering engine itself.</p>"
               "<p class=\"hint\">Try typing <code>about:penguin</code> if you get bored.</p>")
        .arg(rows);
}

QString versionBody(const QString &appVersion, bool isPrivate)
{
    return QStringLiteral(
               "<h1>SpacePenguin %1</h1>"
               "<table class=\"facts\">"
               "<tr><th>Version</th><td>%1</td></tr>"
               "<tr><th>Build</th><td>%2</td></tr>"
               "<tr><th>Qt</th><td>%3 (built against %4)</td></tr>"
               "<tr><th>Profile</th><td>%5</td></tr>"
               "<tr><th>Renderer</th><td>Chromium via QtWebEngine</td></tr>"
               "</table>"
               "<p class=\"hint\">%6</p>")
        .arg(escape(appVersion), escape(QStringLiteral(SPACEPENGUIN_BUILD_TYPE)),
             QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR),
             isPrivate ? QStringLiteral("Private — nothing is written to disk")
                       : QStringLiteral("Persistent"),
             isPrivate ? QStringLiteral("You opened a private window. Session, cookies and cache "
                                       "are discarded when the last private window closes.")
                       : QStringLiteral("This window keeps a session profile on disk."));
}

QString licenseBody()
{
    return QStringLiteral(
        "<h1>Licenses</h1>"
        "<h2>SpacePenguin</h2>"
        "<p>BSD 3-Clause. The full text ships in the <code>LICENSE</code> file at the root of the "
        "repository.</p>"
        "<h2>Qt</h2>"
        "<p>Qt %1 is used under the GNU Lesser General Public License v3, the GNU General Public "
        "License v3, or a commercial license, at your option. QtWebEngine embeds Chromium, which "
        "is covered by its own BSD-style license and patent grants.</p>"
        "<p class=\"hint\">No telemetry, no crash reporting, no network calls except the pages you "
        "ask for.</p>")
        .arg(QStringLiteral(QT_VERSION_STR));
}

QString penguinBody()
{
    return QStringLiteral(
        "<h1>about:penguin</h1>"
        "<pre class=\"art\">"
        "        .--.        \n"
        "       |o_o |       \n"
        "       |:_/ |       \n"
        "      //   \\ \\      \n"
        "     (|     | )     \n"
        "    /'\\_   _/`\\     \n"
        "    \\___)=(___/     \n"
        "</pre>"
        "<p>There is no place like 127.0.0.1.</p>"
        "<p class=\"hint\">Dressed for the southern ocean, running on a laptop in a room you are "
        "not currently in.</p>");
}

QString teapotBody()
{
    return QStringLiteral(
        "<h1>418 I'm a teapot</h1>"
        "<p>This browser will happily render your pages, but it will not brew coffee, and it is "
        "not going to pretend to.</p>"
        "<p class=\"hint\">RFC 2324 &mdash; Hyper Text Coffee Pot Control Protocol. The status "
        "code has been correct since 1998; the coffee still is not.</p>");
}

QString panBody()
{
    return QStringLiteral(
        "<h1>You have been panned</h1>"
        "<p>Every URL is a bread. This one is in the pan, and the pan is on the heat.</p>"
        "<p class=\"hint\">An homage to the browser that hid an easter egg behind a misspelled "
        "word. Ours is spelled correctly, which feels like cheating.</p>");
}

} // namespace

QList<AboutPage> AboutPages::all()
{
    QList<AboutPage> pages;
    for (const PageDefinition &definition : kPages) {
        pages.append(AboutPage{QString::fromLatin1(definition.id),
                               QString::fromLatin1(definition.title),
                               QString::fromLatin1(definition.summary), definition.easterEgg});
    }
    return pages;
}

QStringList AboutPages::ids()
{
    QStringList result;
    for (const AboutPage &page : all())
        result.append(page.id);
    return result;
}

AboutPage AboutPages::page(const QString &id)
{
    for (const AboutPage &page : all()) {
        if (page.id.compare(id, Qt::CaseInsensitive) == 0)
            return page;
    }
    return AboutPage{};
}

bool AboutPages::isKnown(const QString &id)
{
    return !page(id).id.isEmpty();
}

QString AboutPages::pageTitle(const QString &id)
{
    const AboutPage page = AboutPages::page(id);
    if (!page.id.isEmpty())
        return page.title;
    return QStringLiteral("Page not found");
}

QString AboutPages::render(const QString &id, const QString &appVersion, bool isPrivate)
{
    const QString normalized = id.trimmed().toLower();

    if (normalized == QLatin1String("about:about"))
        return htmlShell(pageTitle(id), aboutAboutBody());
    if (normalized == QLatin1String("about:version"))
        return htmlShell(pageTitle(id), versionBody(appVersion, isPrivate));
    if (normalized == QLatin1String("about:license"))
        return htmlShell(pageTitle(id), licenseBody());
    if (normalized == QLatin1String("about:penguin"))
        return htmlShell(pageTitle(id), penguinBody());
    if (normalized == QLatin1String("about:teapot"))
        return htmlShell(pageTitle(id), teapotBody());
    if (normalized == QLatin1String("about:pan"))
        return htmlShell(pageTitle(id), panBody());

    return renderUnknown(id);
}

QString AboutPages::renderUnknown(const QString &id)
{
    const QString body = QStringLiteral("<h1>No such page</h1>"
                                        "<p><code>%1</code> is not an internal SpacePenguin "
                                        "page.</p>"
                                        "<p><a href=\"about:about\">about:about</a> lists the "
                                        "ones that exist.</p>")
                             .arg(escape(id));
    return htmlShell(QStringLiteral("Page not found"), body);
}

QString AboutPages::htmlShell(const QString &pageTitle, const QString &bodyHtml)
{
    return QStringLiteral(
               "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
               "<title>%1 — SpacePenguin</title>"
               "<style>"
               "body{margin:0;min-height:100vh;display:flex;align-items:center;"
               "justify-content:center;padding:3rem 1.5rem;box-sizing:border-box;"
               "font:16px/1.6 system-ui,-apple-system,'Segoe UI',sans-serif;"
               "background:#f6f7f9;color:#14161a}"
               "main{max-width:44rem;width:100%}"
               "h1{font-size:2rem;margin:0 0 1rem;letter-spacing:-0.01em}"
               "h2{font-size:1.1rem;margin:1.75rem 0 0.5rem}"
               "p{margin:0 0 1rem}"
               ".lead{opacity:0.75}"
               ".hint{opacity:0.6;font-size:0.9rem;margin-top:2rem}"
               "a{color:#2f6fed}"
               "code{font-family:ui-monospace,'SF Mono',Menlo,monospace;font-size:0.9em;"
               "background:rgba(20,22,26,0.06);padding:0.1em 0.35em;border-radius:0.25em}"
               "table{border-collapse:collapse;width:100%%}"
               "th,td{text-align:left;padding:0.5rem 0.75rem;border-bottom:1px solid "
               "rgba(20,22,26,0.12);vertical-align:top}"
               "table.facts th{width:9rem;opacity:0.6;font-weight:600}"
               "pre.art{font-family:ui-monospace,'SF Mono',Menlo,monospace;line-height:1.15;"
               "background:rgba(20,22,26,0.06);padding:1rem 1.25rem;border-radius:0.5rem;"
               "overflow-x:auto}"
               "@media (prefers-color-scheme: dark){"
               "body{background:#14161a;color:#f6f7f9}"
               "a{color:#7aa7ff}"
               "code,pre.art{background:rgba(246,247,249,0.08)}"
               "th,td{border-bottom-color:rgba(246,247,249,0.14)}}"
               "</style></head><body><main>%2</main></body></html>")
        .arg(escape(pageTitle), bodyHtml);
}

}
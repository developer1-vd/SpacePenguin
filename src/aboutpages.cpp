#include "aboutpages.h"

#include <QCoreApplication>
#include <QStringList>
#include <QtGlobal>

#include <iterator>

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
    { "about:downloads", "Downloads",
      "View and manage your downloads.", false },
    { "about:history", "History",
      "View your browsing history.", false },
    { "about:extensions", "Extensions",
      "Manage your user scripts and extensions.", false },
    { "about:logins", "Saved Logins",
      "Search, add, edit and delete saved usernames and passwords.", false },
    { "about:settings", "Settings",
      "Configure SpacePenguin.", false },
};

QString escape(const QString &text)
{
    return text.toHtmlEscaped();
}

QString buildRow(const AboutPage &page)
{
    return QStringLiteral("<tr><td><a href=\"%1\">%1</a></td><td>%2</td>"
                          "<td class=\"tag\">%3</td></tr>")
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

QString versionBody(const AboutPageContext &context)
{
    const auto row = [](const QString &label, const QString &value) {
        return QStringLiteral("<tr><th>%1</th><td>%2</td></tr>")
            .arg(escape(label), escape(value));
    };

    QString blockingValue = context.blockingEnabled
        ? QStringLiteral("%1 rules loaded — %2 requests blocked")
              .arg(context.filterRules)
              .arg(context.blockedRequests)
        : QStringLiteral("Disabled — %1 rules loaded").arg(context.filterRules);

    const QStringList rows = {
        row(QStringLiteral("Version"), context.appVersion),
        row(QStringLiteral("Build"), context.buildType),
        row(QStringLiteral("Qt"), QStringLiteral("%1 (built against %2)")
                                       .arg(QString::fromLatin1(qVersion()),
                                            QStringLiteral(QT_VERSION_STR))),
        row(QStringLiteral("Platform"),
            context.platform.isEmpty() ? QStringLiteral("Unknown") : context.platform),
        row(QStringLiteral("Theme"), context.theme),
        row(QStringLiteral("Profile"),
            context.isPrivate ? QStringLiteral("Private — nothing is written to disk")
                              : QStringLiteral("Persistent")),
        row(QStringLiteral("Blocking"), blockingValue),
        row(QStringLiteral("Element hiding"),
            QStringLiteral("%1 cosmetic rules").arg(context.cosmeticRules)),
        row(QStringLiteral("Extensions"),
            QStringLiteral("%1 installed").arg(context.extensionCount)),
        row(QStringLiteral("Renderer"), QStringLiteral("Chromium via QtWebEngine")),
        row(QStringLiteral("Sandbox"),
            context.sandbox.isEmpty() ? QStringLiteral("Unknown") : context.sandbox),
        row(QStringLiteral("Device"),
            context.device.isEmpty() ? QStringLiteral("Unknown") : context.device),
        row(QStringLiteral("Tuning"), context.deviceAdjustments.isEmpty()
                                          ? QStringLiteral("None — Chromium's defaults")
                                          : context.deviceAdjustments),
    };

    return QStringLiteral("<h1>SpacePenguin %1</h1>"
                          "<table class=\"facts\">%2</table>"
                          "<p class=\"hint\">%3</p>")
        .arg(escape(context.appVersion), rows.join(QString()),
             context.isPrivate
                 ? QStringLiteral("You opened a private window. Session, cookies and cache "
                                  "are discarded when the last private window closes.")
                 : QStringLiteral("This window keeps a session profile on disk."));
}

QString licenseBody()
{
    return QStringLiteral(
        "<h1>Licenses</h1>"
        "<h2>SpacePenguin</h2>"
        "<p>GNU General Public License v3. The full text ships in the <code>LICENSE</code> "
        "file at the root of the repository.</p>"
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

bool isWebUrl(const QString &url)
{
    return url.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)
        || url.startsWith(QLatin1String("http://"), Qt::CaseInsensitive);
}

QString entryLink(const AboutPageEntry &entry)
{
    const QString label = entry.title.trimmed().isEmpty() ? entry.url : entry.title;
    // Only web addresses become links; anything else is shown as text.
    if (!isWebUrl(entry.url))
        return escape(label);
    return QStringLiteral("<a href=\"%1\">%2</a>").arg(escape(entry.url), escape(label));
}

QString entryTable(const QList<AboutPageEntry> &entries, const QString &detailHeading)
{
    QString rows;
    for (const AboutPageEntry &entry : entries) {
        rows += QStringLiteral("<tr><td>%1<div class=\"url\">%2</div></td><td class=\"detail\">%3</td></tr>")
                    .arg(entryLink(entry), escape(entry.url), escape(entry.detail));
    }
    return QStringLiteral("<table class=\"entries\"><thead><tr><th>Page</th><th>%1</th></tr></thead>"
                          "<tbody>%2</tbody></table>")
        .arg(escape(detailHeading), rows);
}

QString shortcutHint(const AboutPageContext &context, const QString &command,
                     const QString &fallback)
{
    return QStringLiteral("<kbd>%1</kbd>").arg(escape(context.shortcuts.value(command, fallback)));
}

QString downloadsBody(const AboutPageContext &context)
{
    QString body = QStringLiteral("<h1>Downloads</h1>");
    if (context.downloads.isEmpty()) {
        body += QStringLiteral("<p class=\"empty\">Nothing downloaded in this session yet. Files you "
                               "download appear here.</p>");
    } else {
        body += entryTable(context.downloads, QStringLiteral("Status"));
    }
    body += QStringLiteral("<p class=\"hint\">Press %1 to open the downloads window, where you "
                           "can cancel, open and clear files.</p>")
                .arg(shortcutHint(context, QStringLiteral("downloads"), QStringLiteral("Ctrl+J")));
    return body;
}

QString historyBody(const AboutPageContext &context)
{
    QString body = QStringLiteral("<h1>History</h1>");
    if (context.isPrivate) {
        body += QStringLiteral("<p class=\"empty\">This is a private window. Nothing you visit "
                               "here is recorded.</p>");
    } else if (context.history.isEmpty()) {
        body += QStringLiteral("<p class=\"empty\">No history yet. Pages you visit appear here.</p>");
    } else {
        body += entryTable(context.history, QStringLiteral("Last visited"));
    }
    body += QStringLiteral("<p class=\"hint\">Press %1 to search, delete or clear history.</p>")
                .arg(shortcutHint(context, QStringLiteral("history"), QStringLiteral("Ctrl+H")));
    return body;
}

QString extensionsBody(const AboutPageContext &context)
{
    QString body = QStringLiteral(
        "<h1>Extensions</h1>");

    if (context.extensionCount == 0) {
        body += QStringLiteral(
            "<p>No user scripts installed.</p>"
            "<p>You can install scripts via the Extensions dialog or by placing "
            "<code>.js</code> files in your extensions directory.</p>");
    } else {
        body += QStringLiteral(
            "<p><b>%1</b> user script%2 installed.</p>")
            .arg(context.extensionCount)
            .arg(context.extensionCount == 1 ? QStringLiteral("") : QStringLiteral("s"));
    }

    body += QStringLiteral("<p class=\"hint\">Press %1 to manage extensions.</p>")
                .arg(shortcutHint(context, QStringLiteral("extensions"),
                                  QStringLiteral("Ctrl+Shift+E")));

    return body;
}

QString settingsBody(const AboutPageContext &context)
{
    return QStringLiteral(
               "<h1>Settings</h1>"
               "<p>The settings window opens on top of this page.</p>"
               "<p class=\"hint\">Press %1 any time, or use <b>Settings</b> in the main "
               "menu.</p>")
        .arg(shortcutHint(context, QStringLiteral("settings"), QStringLiteral("Ctrl+,")));
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

QString AboutPages::render(const QString &id, const AboutPageContext &context)
{
    const QString normalized = id.trimmed().toLower();

    if (normalized == QLatin1String("about:about"))
        return htmlShell(pageTitle(id), aboutAboutBody());
    if (normalized == QLatin1String("about:version"))
        return htmlShell(pageTitle(id), versionBody(context));
    if (normalized == QLatin1String("about:license"))
        return htmlShell(pageTitle(id), licenseBody());
    if (normalized == QLatin1String("about:penguin"))
        return htmlShell(pageTitle(id), penguinBody());
    if (normalized == QLatin1String("about:teapot"))
        return htmlShell(pageTitle(id), teapotBody());
    if (normalized == QLatin1String("about:pan"))
        return htmlShell(pageTitle(id), panBody());
    if (normalized == QLatin1String("about:downloads"))
        return htmlShell(pageTitle(id), downloadsBody(context));
    if (normalized == QLatin1String("about:history"))
        return htmlShell(pageTitle(id), historyBody(context));
    if (normalized == QLatin1String("about:extensions"))
        return htmlShell(pageTitle(id), extensionsBody(context));
    if (normalized == QLatin1String("about:settings"))
        return htmlShell(pageTitle(id), settingsBody(context));
    // The window opens about:logins as sp://logins; this is only its signpost.
    if (normalized == QLatin1String("about:logins"))
        return htmlShell(pageTitle(id), QStringLiteral("<h1>Saved Logins</h1>"
                                                       "<p><a href=\"sp://logins/\">Open saved logins</a></p>"));

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
               "<meta name=\"color-scheme\" content=\"light dark\">"
               "<title>%1 — SpacePenguin</title>"
               "<style>"
               ":root{--bg:#f4f6fb;--card:#fff;--text:#1b1e25;--muted:#5f6675;"
               "--line:rgba(27,30,37,.1);--accent:#2f5fe8;--chip:rgba(27,30,37,.06)}"
               "@media (prefers-color-scheme: dark){:root{--bg:#15171d;--card:#22252d;"
               "--text:#e9ebf1;--muted:#9aa1b1;--line:rgba(233,235,241,.1);--accent:#82a0ff;"
               "--chip:rgba(233,235,241,.08)}}"
               // The browser's own theme, when it is not following the system.
               ":root.light{color-scheme:light;--bg:#f4f6fb;--card:#fff;--text:#1b1e25;"
               "--muted:#5f6675;--line:rgba(27,30,37,.1);--accent:#2f5fe8;--chip:rgba(27,30,37,.06)}"
               ":root.dark{color-scheme:dark;--bg:#15171d;--card:#22252d;--text:#e9ebf1;"
               "--muted:#9aa1b1;--line:rgba(233,235,241,.1);--accent:#82a0ff;"
               "--chip:rgba(233,235,241,.08)}"
               "*{box-sizing:border-box}"
               "body{margin:0;min-height:100vh;padding:3.5rem 1.5rem;"
               "font:15px/1.6 system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;"
               "background:var(--bg);color:var(--text)}"
               "header{max-width:52rem;margin:0 auto 1.25rem;display:flex;align-items:center;"
               "gap:.6rem;color:var(--muted);font-weight:600;letter-spacing:.02em}"
               "header svg{width:28px;height:28px}"
               "main{max-width:52rem;margin:0 auto;background:var(--card);border:1px solid var(--line);"
               "border-radius:18px;padding:2.25rem 2.5rem}"
               "h1{font-size:1.9rem;margin:0 0 1rem;letter-spacing:-.02em}"
               "h2{font-size:1.05rem;margin:2rem 0 .5rem}"
               "p{margin:0 0 1rem}"
               ".lead{color:var(--muted);font-size:1.05rem}"
               ".hint{color:var(--muted);font-size:.9rem;margin-top:2rem}"
               ".empty{color:var(--muted);padding:1.5rem;border:1px dashed var(--line);"
               "border-radius:12px;text-align:center}"
               "a{color:var(--accent);text-decoration:none}a:hover{text-decoration:underline}"
               "code,kbd{font-family:ui-monospace,'SF Mono',Menlo,Consolas,monospace;font-size:.88em;"
               "background:var(--chip);padding:.1em .4em;border-radius:6px}"
               "kbd{border:1px solid var(--line);border-bottom-width:2px}"
               "blockquote{margin:0 0 1rem;padding:.25rem 1rem;border-left:3px solid var(--accent);"
               "color:var(--muted)}"
               "table{border-collapse:collapse;width:100%}"
               "th,td{text-align:left;padding:.55rem .75rem;border-bottom:1px solid var(--line);"
               "vertical-align:top}"
               "thead th{color:var(--muted);font-size:.8rem;text-transform:uppercase;"
               "letter-spacing:.06em}"
               "table.facts th{width:10rem;color:var(--muted);font-weight:600}"
               "td.tag{white-space:nowrap;color:var(--muted);font-size:.8rem}"
               "table.entries .url{color:var(--muted);font-size:.8rem;word-break:break-all}"
               "table.entries .detail{white-space:nowrap;color:var(--muted);font-size:.85rem}"
               "pre.art{font-family:ui-monospace,'SF Mono',Menlo,Consolas,monospace;line-height:1.15;"
               "background:var(--chip);padding:1rem 1.25rem;border-radius:12px;overflow-x:auto}"
               "</style></head><body>"
               "<header><svg viewBox=\"0 0 100 100\" aria-hidden=\"true\"><clipPath id=\"g\">"
               "<circle cx=\"50\" cy=\"72\" r=\"20\"/></clipPath>"
               "<g transform=\"translate(50 50) scale(.92) translate(-50 -53.25)\">"
               "<path d=\"M25.5 54V35a24.5 24.5 0 0 1 49 0v19\" fill=\"none\" stroke=\"#ff7a1a\" stroke-width=\"7\"/>"
               "<path d=\"M32 47V35a18 18 0 0 1 36 0v12h1a14 14 0 0 1 14 14v22a14 14 0 0 1-14 14H31a14 14 0 0 1-14-14V61a14 14 0 0 1 14-14z\" fill=\"#16171d\" stroke=\"#fff\" stroke-width=\"5\" stroke-linejoin=\"round\" paint-order=\"stroke\"/>"
               "<path d=\"M37 47V36a13 13 0 0 1 26 0v11z\" fill=\"#fff\"/>"
               "<circle cx=\"45\" cy=\"34\" r=\"2.6\" fill=\"#16171d\"/>"
               "<circle cx=\"55\" cy=\"34\" r=\"2.6\" fill=\"#16171d\"/>"
               "<path d=\"M47 38.5h6l-3 4z\" fill=\"#ffb21f\" stroke=\"#ffb21f\" stroke-width=\"1.5\" stroke-linejoin=\"round\"/>"
               "<circle cx=\"50\" cy=\"72\" r=\"20\" fill=\"#6a4fe0\"/>"
               "<path d=\"M33 64c4-3 10-3 13 0 2 3-1 5-1 8 0 4 4 6 3 10-1 5-6 7-9 5-3-2-2-7-5-10-3-3-6-6-5-9 1-2 2-3 4-4zM56 61c5-1 11 1 13 5 1 3-1 5-4 5-3 1-3 4-2 7 2 4 1 8-3 10-4 1-5-3-6-7-1-5-4-8-4-13 0-4 2-6 6-7z\" fill=\"#2fb7a3\" clip-path=\"url(#g)\"/>"
               "</g></svg>"
               "SpacePenguin</header>"
               "<main>%2</main></body></html>")
        .arg(escape(pageTitle), bodyHtml);
}

}

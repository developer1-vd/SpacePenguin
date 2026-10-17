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
    { "about:force", "Use the Force",
      "May it be with you. Always.", true },
    { "about:deathstar", "That's No Moon",
      "A fully operational battle station.", true },
    { "about:yoda", "Wise, You Are",
      "Do. Or do not. There is no try.", true },
    { "about:hyperdrive", "Punch It",
      "Making the jump to lightspeed.", true },
    { "about:mozilla", "Book of Mozilla",
      "An ancient text describing the evolution of browsers, verse by verse.", true },
    { "about:firefox", "Phoenix Rising",
      "From the ashes of Netscape, a browser for everyone.", true },
    { "about:servo", "Fearless Concurrency",
      "A browser engine written in Rust, parallel by default.", true },
    { "about:rust", "Empowering Everyone",
      "Build reliable and efficient software.", true },
    { "about:downloads", "Downloads",
      "View and manage your downloads.", false },
    { "about:history", "History",
      "View your browsing history.", false },
    { "about:extensions", "Extensions",
      "Manage your user scripts and extensions.", false },
    { "about:settings", "Settings",
      "Configure SpacePenguin.", false },
    { "about:tatooine", "Tatooine",
      "If there's a bright center to the universe, you're on the planet that it's farthest from.", true },
    { "about:hoth", "Hoth",
      "Climate: frozen. Indigenous life: tauntauns, wampas, and rebel bases.", true },
    { "about:dagobah", "Dagobah",
      "Strong with the Force, this place is. Much to learn, you still have.", true },
    { "about:endor", "Endor",
      "Home of the Ewoks. Cute, fuzzy, and surprisingly good at taking down AT-STs.", true },
    { "about:naboo", "Naboo",
      "Peaceful planet. Beautiful architecture. Gungan neighbors. Underwater cities.", true },
    { "about:coruscant", "Coruscant",
      "The entire planet is a city. Traffic is terrible. Senate meets here.", true },
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

QString forceBody()
{
    return QStringLiteral(
        "<h1>Use the Force</h1>"
        "<pre class=\"art\">"
        "  \\ | /     \n"
        "  -- * --    \n"
        "  / | \\      \n"
        "            \n"
        "  ~ ~ ~ ~ ~  \n"
        "</pre>"
        "<p>The Force is what gives a browser its power. It surrounds us, penetrates us, "
        "binds the galaxy together... and makes page loads feel snappy.</p>"
        "<p class=\"hint\">For a more elegant weapon from a more civilized age, try "
        "<code>about:yoda</code>.</p>");
}

QString deathstarBody()
{
    return QStringLiteral(
        "<h1>That's No Moon</h1>"
        "<pre class=\"art\">"
        "     .--.    \n"
        "    /      \\  \n"
        "   |  ( )  |  \n"
        "    \\  __  /  \n"
        "     '--'    \n"
        "</pre>"
        "<p>A fully operational battle station. It has the power to destroy a planet, "
        "or at least your productivity.</p>"
        "<p class=\"hint\">The thermal exhaust port is two meters wide. "
        "Womp rats not included.</p>");
}

QString yodaBody()
{
    return QStringLiteral(
        "<h1>Wise, You Are</h1>"
        "<pre class=\"art\">"
        "      .--.   \n"
        "     /    \\  \n"
        "    |  ::  |  \n"
        "     \\ __ /   \n"
        "       ||     \n"
        "       ||     \n"
        "</pre>"
        "<p>Do. Or do not. There is no try.</p>"
        "<p>Clear your mind must be, if you are to find the bugs in your code.</p>"
        "<p class=\"hint\">Patience you must have, young padawan. "
        "Clear the cache, you must.</p>");
}

QString hyperdriveBody()
{
    return QStringLiteral(
        "<h1>Punch It</h1>"
        "<pre class=\"art\">"
        "    * * * * * * *   \n"
        "   * * * * * * * *  \n"
        "  * * * * * * * * * \n"
        " * * * * * * * * * *\n"
        "  * * * * * * * * * \n"
        "   * * * * * * * *  \n"
        "    * * * * * * *   \n"
        "</pre>"
        "<p>Making the jump to lightspeed. Warp factor 9.9 engaged.</p>"
        "<p>Your tabs are now streaking through hyperspace. </p>"
        "<p class=\"hint\">Navicomputer calculates: 12 parsecs to Kessel. "
        "The Falcon did it in less than twelve.</p>");
}

QString mozillaBody()
{
    return QStringLiteral(
        "<h1>Book of Mozilla</h1>"
        "<pre class=\"art\">"
        "      __    \n"
        "     /  \\   \n"
        "    | :: |   \n"
        "     \\__/    \n"
        "    /    \\   \n"
        "   /______\\  \n"
        "</pre>"
        "<p><b>Chapter 7, Verse 47</b></p>"
        "<blockquote><p>The beast rose from the sea, and its mark was 666. "
        "But mankind, in its pride and folly, had already built its own destruction. "
        "The browser wars had begun, and only one would survive.</p></blockquote>"
        "<p><b>Chapter 8, Verse 1</b></p>"
        "<blockquote><p>And it came to pass that the survivor was reborn, "
        "rising from the ashes of Netscape, clothed in open source, "
        "to bring light unto the darkness of the web.</p></blockquote>"
        "<p><b>Chapter 9, Verse 3</b></p>"
        "<blockquote><p>But lo, the reborn browser was tempted by the siren song of "
        "proprietary software, and it was torn asunder. From its fragments arose "
        "a phoenix, and it was called Firefox, and it brought cross-platform salvation.</p></blockquote>"
        "<p><b>Chapter 10, Verse 5</b></p>"
        "<blockquote><p>And the phoenix multiplied, and its offspring spread across "
        "every desktop and mobile device. But in its success, it forgot the old ways, "
        "and the gears of progress turned ever faster.</p></blockquote>"
        "<p class=\"hint\">Mozilla. Mankind's last hope for an open web. "
        "Since 1998.</p>");
}

QString firefoxBody()
{
    return QStringLiteral(
        "<h1>Phoenix Rising</h1>"
        "<pre class=\"art\">"
        "      /\\      \n"
        "     /  \\     \n"
        "    /____\\    \n"
        "   /      \\   \n"
        "  /________\\  \n"
        "   \\  ||  /   \n"
        "    \\||/     \n"
        "</pre>"
        "<p>From the ashes of Netscape, a browser for everyone.</p>"
        "<p>Built on Gecko, shaped by community, guided by principles.</p>"
        "<p class=\"hint\">Your browser, your rules. Since 2002.</p>");
}

QString servoBody()
{
    return QStringLiteral(
        "<h1>Fearless Concurrency</h1>"
        "<pre class=\"art\">"
        "  // || \\\\   \n"
        " //  ||  \\\\  \n"
        "///  ||  \\\\\\ \n"
        "      ||      \n"
        "      ||      \n"
        "</pre>"
        "<p>A browser engine written in Rust, parallel by default.</p>"
        "<p>Memory safety without garbage collection. Fearless parallelism.</p>"
        "<p class=\"hint\">Racing the future, one thread at a time.</p>");
}

QString rustBody()
{
    return QStringLiteral(
        "<h1>Empowering Everyone</h1>"
        "<pre class=\"art\">"
        "   .--.       \n"
        "  /  .\\      \n"
        "  |  | |      \n"
        "  \\  / /      \n"
        "   '--'       \n"
        "</pre>"
        "<p>A language for building reliable and efficient software.</p>"
        "<p>Zero-cost abstractions. Move semantics. Fearless concurrency.</p>"
        "<p class=\"hint\">If it compiles, it probably works. Mostly.</p>");
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

bool isStarWarsPage(const QString &normalized)
{
    static const char *starWarsIds[] = {
        "about:tatooine", "about:hoth", "about:dagobah",
        "about:endor", "about:naboo", "about:coruscant",
        nullptr
    };
    for (int i = 0; starWarsIds[i]; ++i) {
        if (normalized == QLatin1String(starWarsIds[i]))
            return true;
    }
    return false;
}

QString starWarsBody(const QString &id)
{
    const AboutPage page = AboutPages::page(id);
    if (page.id.isEmpty())
        return QString();

    QStringList hints = {
        QStringLiteral("May the Force be with you."),
        QStringLiteral("Do. Or do not. There is no try."),
        QStringLiteral("I find your lack of faith disturbing."),
        QStringLiteral("These aren't the droids you're looking for."),
        QStringLiteral("I've got a bad feeling about this."),
        QStringLiteral("It's a trap!"),
        QStringLiteral("Hello there."),
        QStringLiteral("That's no moon. That's a space station."),
        QStringLiteral("Fear leads to the dark side."),
        QStringLiteral("Truly wonderful, the mind of a child is."),
        QStringLiteral("The Force is strong with this one."),
        QStringLiteral("Size matters not."),
        QStringLiteral("Always in motion is the future."),
        QStringLiteral("The Force will be with you. Always."),
        QStringLiteral("Difficult, this age-old relation between teacher and pupil."),
    };

    const int hintIndex = qHash(id) % hints.size();

    return QStringLiteral(
        "<h1>%1</h1>"
        "<p>%2</p>"
        "<p class=\"hint\">%3</p>")
        .arg(escape(page.title), escape(page.summary), hints[hintIndex]);

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
    if (normalized == QLatin1String("about:force"))
        return htmlShell(pageTitle(id), forceBody());
    if (normalized == QLatin1String("about:deathstar"))
        return htmlShell(pageTitle(id), deathstarBody());
    if (normalized == QLatin1String("about:yoda"))
        return htmlShell(pageTitle(id), yodaBody());
    if (normalized == QLatin1String("about:hyperdrive"))
        return htmlShell(pageTitle(id), hyperdriveBody());
    if (normalized == QLatin1String("about:mozilla"))
        return htmlShell(pageTitle(id), mozillaBody());
    if (normalized == QLatin1String("about:firefox"))
        return htmlShell(pageTitle(id), firefoxBody());
    if (normalized == QLatin1String("about:servo"))
        return htmlShell(pageTitle(id), servoBody());
    if (normalized == QLatin1String("about:rust"))
        return htmlShell(pageTitle(id), rustBody());
    if (normalized == QLatin1String("about:downloads"))
        return htmlShell(pageTitle(id), downloadsBody(context));
    if (normalized == QLatin1String("about:history"))
        return htmlShell(pageTitle(id), historyBody(context));
    if (normalized == QLatin1String("about:extensions"))
        return htmlShell(pageTitle(id), extensionsBody(context));
    if (normalized == QLatin1String("about:settings"))
        return htmlShell(pageTitle(id), settingsBody(context));
    if (isStarWarsPage(normalized))
        return htmlShell(pageTitle(id), starWarsBody(id));

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

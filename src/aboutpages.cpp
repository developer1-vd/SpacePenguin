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

QString downloadsBody()
{
    return QStringLiteral(
        "<h1>Downloads</h1>"
        "<p>Downloads will appear here as you download files.</p>"
        "<p class=\"hint\">Press <kbd>Ctrl</kbd>+<kbd>J</kbd> to open this page quickly.</p>");
}

QString historyBody()
{
    return QStringLiteral(
        "<h1>History</h1>"
        "<p>Your browsing history will appear here.</p>"
        "<p class=\"hint\">Press <kbd>Ctrl</kbd>+<kbd>H</kbd> to open this page quickly. "
        "Or use <code>Tools → Settings</code> to configure history.</p>");
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

    body += QStringLiteral(
        "<p class=\"hint\">Press <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>E</kbd> to "
        "manage extensions.</p>");

    return body;
}

QString settingsBody()
{
    return QStringLiteral(
        "<h1>Settings</h1>"
        "<p>Open the Settings dialog to configure SpacePenguin.</p>"
        "<p class=\"hint\">Press <kbd>Ctrl</kbd>+<kbd>,</kbd> or use "
        "<code>Tools → Settings</code>.</p>");
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
        return htmlShell(pageTitle(id), downloadsBody());
    if (normalized == QLatin1String("about:history"))
        return htmlShell(pageTitle(id), historyBody());
    if (normalized == QLatin1String("about:extensions"))
        return htmlShell(pageTitle(id), extensionsBody(context));
    if (normalized == QLatin1String("about:settings"))
        return htmlShell(pageTitle(id), settingsBody());
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
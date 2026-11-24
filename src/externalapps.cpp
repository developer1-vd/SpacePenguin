#include "externalapps.h"

#include "sandbox.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

namespace spacepenguin::ExternalApps {

namespace {

const QString kAllowedKey = QStringLiteral("externalApps/alwaysOpen");

bool isWellFormedScheme(const QString &scheme)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z][a-z0-9+.-]{0,63}$"));
    return pattern.match(scheme).hasMatch();
}

/** The web's own schemes and the browser's, which are pages, not apps. */
bool isWebScheme(const QString &scheme)
{
    static const QStringList web = {
        QStringLiteral("http"), QStringLiteral("https"), QStringLiteral("ws"), QStringLiteral("wss"),
        QStringLiteral("ftp"),
    };
    return web.contains(scheme);
}

QString knownName(const QString &scheme)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("roblox"), QStringLiteral("Roblox")},
        {QStringLiteral("roblox-player"), QStringLiteral("Roblox")},
        {QStringLiteral("roblox-studio"), QStringLiteral("Roblox Studio")},
        {QStringLiteral("discord"), QStringLiteral("Discord")},
        {QStringLiteral("zoommtg"), QStringLiteral("Zoom")},
        {QStringLiteral("zoomus"), QStringLiteral("Zoom")},
        {QStringLiteral("msteams"), QStringLiteral("Microsoft Teams")},
        {QStringLiteral("slack"), QStringLiteral("Slack")},
        {QStringLiteral("spotify"), QStringLiteral("Spotify")},
        {QStringLiteral("steam"), QStringLiteral("Steam")},
        {QStringLiteral("vscode"), QStringLiteral("Visual Studio Code")},
        {QStringLiteral("vscodium"), QStringLiteral("VSCodium")},
        {QStringLiteral("obsidian"), QStringLiteral("Obsidian")},
        {QStringLiteral("notion"), QStringLiteral("Notion")},
        {QStringLiteral("figma"), QStringLiteral("Figma")},
        {QStringLiteral("tg"), QStringLiteral("Telegram")},
        {QStringLiteral("whatsapp"), QStringLiteral("WhatsApp")},
        {QStringLiteral("signal"), QStringLiteral("Signal")},
        {QStringLiteral("element"), QStringLiteral("Element")},
        {QStringLiteral("magnet"), QCoreApplication::translate("ExternalApps", "your torrent app")},
        {QStringLiteral("mailto"), QCoreApplication::translate("ExternalApps", "your email app")},
        {QStringLiteral("tel"), QCoreApplication::translate("ExternalApps", "your phone app")},
        {QStringLiteral("sms"), QCoreApplication::translate("ExternalApps", "your messages app")},
        {QStringLiteral("appstream"), QCoreApplication::translate("ExternalApps", "your software centre")},
        {QStringLiteral("itms-apps"), QStringLiteral("App Store")},
        {QStringLiteral("macappstore"), QStringLiteral("App Store")},
        {QStringLiteral("ms-windows-store"), QStringLiteral("Microsoft Store")},
    };
    return names.value(scheme);
}

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
/** The .desktop file the system opens @p scheme with, or empty: xdg-mime. */
QString defaultDesktopFile(const QString &scheme)
{
    static QHash<QString, QString> cache;
    const auto cached = cache.constFind(scheme);
    if (cached != cache.constEnd())
        return *cached;
    QString desktop;
    const QString xdgMime = QStandardPaths::findExecutable(QStringLiteral("xdg-mime"));
    if (!xdgMime.isEmpty()) {
        QProcess process;
        process.start(xdgMime, {QStringLiteral("query"), QStringLiteral("default"),
                                QStringLiteral("x-scheme-handler/") + scheme});
        if (process.waitForFinished(1500) && process.exitStatus() == QProcess::NormalExit)
            desktop = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }
    cache.insert(scheme, desktop);
    return desktop;
}

/** Name= of a .desktop file, from the applications folders. */
QString desktopFileName(const QString &desktop)
{
    if (desktop.isEmpty() || desktop.contains(QLatin1Char('/')))
        return {};
    const QString path = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktop);
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream in(&file);
    bool inEntry = false;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith(QLatin1Char('[')))
            inEntry = line == QLatin1String("[Desktop Entry]");
        else if (inEntry && line.startsWith(QLatin1String("Name=")))
            return line.mid(5).trimmed();
    }
    return {};
}

bool canAskTheSystem()
{
    return !Sandbox::isolatedByFlatpak() && !QStandardPaths::findExecutable(QStringLiteral("xdg-mime")).isEmpty();
}
#endif

}

bool isBlocked(const QString &scheme)
{
    static const QStringList blocked = {
        // Code, and the browser's own pages.
        QStringLiteral("javascript"), QStringLiteral("vbscript"), QStringLiteral("data"),
        QStringLiteral("blob"), QStringLiteral("filesystem"), QStringLiteral("about"),
        QStringLiteral("chrome"), QStringLiteral("chrome-extension"), QStringLiteral("devtools"),
        QStringLiteral("view-source"), QStringLiteral("sp"), QStringLiteral("qrc"),
        // Local files and archives.
        QStringLiteral("file"), QStringLiteral("jar"), QStringLiteral("mhtml"),
        QStringLiteral("res"), QStringLiteral("mk"), QStringLiteral("its"), QStringLiteral("ms-its"),
        // Windows' own handlers that attacks have used to run code or fetch files.
        QStringLiteral("shell"), QStringLiteral("hcp"), QStringLiteral("help"),
        QStringLiteral("ms-msdt"), QStringLiteral("search-ms"), QStringLiteral("search"),
        QStringLiteral("ms-officecmd"), QStringLiteral("ms-cxh"), QStringLiteral("ms-cxh-full"),
        QStringLiteral("ms-appinstaller"),
    };
    return blocked.contains(scheme.toLower());
}

QString linkText(const QUrl &url)
{
    if (url.isValid())
        return url.toString(QUrl::FullyEncoded);
    // "…; source was \"discord://-/channels/@me\"; scheme = …"
    static const QRegularExpression source(QStringLiteral("source was \"([^\"\\x00-\\x1f]{1,8192})\"; "));
    const QRegularExpressionMatch match = source.match(url.errorString());
    if (!match.hasMatch())
        return {};
    const QString text = match.captured(1);
    // Only the same link: its scheme, then a colon.
    if (!text.startsWith(url.scheme() + QLatin1Char(':'), Qt::CaseInsensitive))
        return {};
    return text;
}

bool isAppLink(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    return isWellFormedScheme(scheme) && !isWebScheme(scheme) && !isBlocked(scheme) && !linkText(url).isEmpty();
}

bool isRoblox(const QString &scheme)
{
    return scheme == QLatin1String("roblox") || scheme == QLatin1String("roblox-player")
        || scheme == QLatin1String("roblox-studio");
}

QString appName(const QString &scheme)
{
    const QString lower = scheme.toLower();
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
    if (isWellFormedScheme(lower) && canAskTheSystem()) {
        const QString name = desktopFileName(defaultDesktopFile(lower));
        if (!name.isEmpty())
            return name;
    }
#endif
    const QString known = knownName(lower);
    return known.isEmpty() ? QCoreApplication::translate("ExternalApps", "an app (%1:)").arg(lower) : known;
}

bool hasHandler(const QString &scheme)
{
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
    const QString lower = scheme.toLower();
    if (isWellFormedScheme(lower) && canAskTheSystem())
        return !defaultDesktopFile(lower).isEmpty();
#else
    Q_UNUSED(scheme);
#endif
    return true;
}

QString siteOf(const QUrl &page)
{
    return page.host().toLower();
}

bool isAlwaysAllowed(const QString &site, const QString &scheme)
{
    if (site.isEmpty())
        return false;
    return QSettings().value(kAllowedKey).toStringList().contains(site + QLatin1Char(' ') + scheme.toLower());
}

void setAlwaysAllowed(const QString &site, const QString &scheme)
{
    if (site.isEmpty())
        return;
    QSettings settings;
    QStringList allowed = settings.value(kAllowedKey).toStringList();
    const QString entry = site + QLatin1Char(' ') + scheme.toLower();
    if (!allowed.contains(entry)) {
        allowed << entry;
        settings.setValue(kAllowedKey, allowed);
    }
}

bool open(const QUrl &url)
{
    if (!isAppLink(url))
        return false;
    if (url.isValid())
        return QDesktopServices::openUrl(url);
    // A link Qt refuses goes to the system's opener as the page wrote it, as one
    // argument (no shell).
    const QString text = linkText(url);
#if defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("/usr/bin/open"), {text});
#elif defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    const QString xdgOpen = QStandardPaths::findExecutable(QStringLiteral("xdg-open"));
    return !xdgOpen.isEmpty() && QProcess::startDetached(xdgOpen, {text});
#else
    Q_UNUSED(text);
    return false;
#endif
}

FlatpakRef parseFlatpakRef(const QByteArray &contents)
{
    FlatpakRef ref;
    bool inRef = false;
    for (const QByteArray &raw : contents.split('\n')) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inRef = line == QLatin1String("[Flatpak Ref]");
            continue;
        }
        const int equals = line.indexOf(QLatin1Char('='));
        if (!inRef || equals <= 0)
            continue;
        const QString key = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1).trimmed();
        if (key == QLatin1String("Name"))
            ref.name = value;
        else if (key == QLatin1String("Title"))
            ref.title = value;
        else if (key == QLatin1String("Branch"))
            ref.branch = value;
        else if (key == QLatin1String("Url"))
            ref.repository = QUrl(value);
    }
    // Reverse-DNS app IDs only, and repositories on the web.
    static const QRegularExpression appId(QStringLiteral("^[A-Za-z_][A-Za-z0-9_-]*(\\.[A-Za-z0-9_-]+){2,}$"));
    if (!appId.match(ref.name).hasMatch())
        ref.name.clear();
    if (ref.repository.scheme() != QLatin1String("https") && ref.repository.scheme() != QLatin1String("http"))
        ref.repository = QUrl();
    if (ref.title.size() > 100)
        ref.title.truncate(100);
    return ref;
}

bool isFlatpakRef(const QString &path)
{
    return path.endsWith(QLatin1String(".flatpakref"), Qt::CaseInsensitive);
}

bool canInstallFlatpaks()
{
#if defined(Q_OS_LINUX)
    return !Sandbox::isolatedByFlatpak() && !QStandardPaths::findExecutable(QStringLiteral("flatpak")).isEmpty();
#else
    return false;
#endif
}

QUrl soberPage()
{
    return QUrl(QStringLiteral("https://flathub.org/apps/org.vinegarhq.Sober"));
}

}

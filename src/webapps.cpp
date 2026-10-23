#include "webapps.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shlguid.h>
#include <shobjidl.h>
#endif

namespace spacepenguin {
namespace {

const QString kGroup = QStringLiteral("webapps");

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
/** The app's name as a file name: no characters any system refuses. */
QString fileName(const WebApp &app)
{
    QString name = app.name;
    name.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]")));
    name = name.trimmed();
    return name.isEmpty() ? app.id : name;
}
#endif

/** One argument of a .desktop file's Exec line, quoted as the specification asks. */
QString execArgument(QString argument)
{
    argument.replace(QLatin1Char('%'), QStringLiteral("%%"));
    if (!argument.contains(QRegularExpression(QStringLiteral("[\\s\"'\\\\$`<>|;&*?#()~]"))))
        return argument;
    argument.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    argument.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    argument.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    argument.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    return QLatin1Char('"') + argument + QLatin1Char('"');
}

/** The program and the arguments that start this SpacePenguin from a launcher. */
QStringList launcherCommand()
{
    const QString flatpak = qEnvironmentVariable("FLATPAK_ID");
    if (!flatpak.isEmpty())
        return {QStringLiteral("flatpak"), QStringLiteral("run"), flatpak};
    // An AppImage runs from a temporary mount; the launcher must name the file.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty())
        return {appImage};
    // An unpacked AppImage (install.sh) starts through its AppRun, which sets it up.
    const QString appDir = qEnvironmentVariable("APPDIR");
    if (!appDir.isEmpty() && QFileInfo::exists(appDir + QStringLiteral("/AppRun")))
        return {appDir + QStringLiteral("/AppRun")};
    return {QCoreApplication::applicationFilePath()};
}

int iconScore(const QJsonObject &icon)
{
    const QString source = icon.value(QStringLiteral("src")).toString();
    const QString type = icon.value(QStringLiteral("type")).toString();
    if (source.isEmpty() || type.contains(QLatin1String("svg"))
        || source.section(QLatin1Char('?'), 0, 0).endsWith(QLatin1String(".svg"), Qt::CaseInsensitive))
        return -1; // drawn without an SVG reader, so not used
    int size = 0;
    const QStringList sizes = icon.value(QStringLiteral("sizes")).toString().split(QLatin1Char(' '),
                                                                                  Qt::SkipEmptyParts);
    for (const QString &entry : sizes)
        size = qMax(size, entry.section(QLatin1Char('x'), 0, 0, QString::SectionCaseInsensitiveSeps).toInt());
    // Best between 192 and 512 pixels, the bigger the better; then bigger ones, the
    // nearer 512 the better; then smaller ones.
    int score = size >= 192 && size <= 512 ? 3000 + size : size > 512 ? 2000 - qMin(size / 10, 999) : 1000 + size;
    // A "maskable" icon has its picture shrunk into a safe zone; plain ones look better.
    const QString purpose = icon.value(QStringLiteral("purpose")).toString();
    if (!purpose.isEmpty() && !purpose.contains(QLatin1String("any")))
        score -= 2500;
    return qMax(score, 0);
}

#ifdef Q_OS_WIN
bool writeShortcut(const QString &linkPath, const QString &target, const QString &arguments,
                   const QString &iconPath, const QString &description)
{
    const HRESULT initialised = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool saved = false;
    IShellLinkW *link = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                   reinterpret_cast<void **>(&link)))) {
        const QString nativeTarget = QDir::toNativeSeparators(target);
        const QString nativeIcon = QDir::toNativeSeparators(iconPath);
        const QString nativeLink = QDir::toNativeSeparators(linkPath);
        link->SetPath(reinterpret_cast<const wchar_t *>(nativeTarget.utf16()));
        link->SetArguments(reinterpret_cast<const wchar_t *>(arguments.utf16()));
        link->SetDescription(reinterpret_cast<const wchar_t *>(description.utf16()));
        if (!iconPath.isEmpty())
            link->SetIconLocation(reinterpret_cast<const wchar_t *>(nativeIcon.utf16()), 0);
        IPersistFile *file = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file)))) {
            saved = SUCCEEDED(file->Save(reinterpret_cast<const wchar_t *>(nativeLink.utf16()), TRUE));
            file->Release();
        }
        link->Release();
    }
    if (SUCCEEDED(initialised))
        CoUninitialize();
    return saved;
}

QString windowsArgument(const QString &argument)
{
    if (!argument.contains(QLatin1Char(' ')) && !argument.contains(QLatin1Char('"')))
        return argument;
    QString quoted = argument;
    quoted.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}
#endif

#ifdef Q_OS_MACOS
QString shellQuote(QString text)
{
    text.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + text + QLatin1Char('\'');
}
#endif

} // namespace

bool WebApp::contains(const QUrl &url) const
{
    const QUrl base = scope.isValid() ? scope : startUrl;
    if (url.scheme() != base.scheme() || url.host() != base.host() || url.port() != base.port())
        return false;
    return url.path().startsWith(base.path().isEmpty() ? QStringLiteral("/") : base.path());
}

WebApps *WebApps::instance()
{
    static WebApps *apps = new WebApps(QCoreApplication::instance());
    return apps;
}

WebApps::WebApps(QObject *parent)
    : QObject(parent)
{
}

QList<WebApp> WebApps::apps() const
{
    QSettings settings;
    settings.beginGroup(kGroup);
    QList<WebApp> list;
    for (const QString &id : settings.childGroups()) {
        const WebApp entry = app(id);
        if (entry.isValid())
            list.append(entry);
    }
    std::sort(list.begin(), list.end(), [](const WebApp &a, const WebApp &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return list;
}

WebApp WebApps::app(const QString &id) const
{
    QSettings settings;
    settings.beginGroup(kGroup + QLatin1Char('/') + id);
    WebApp entry;
    entry.startUrl = settings.value(QStringLiteral("start")).toUrl();
    if (!entry.startUrl.isValid())
        return WebApp();
    entry.id = id;
    entry.name = settings.value(QStringLiteral("name")).toString();
    entry.scope = settings.value(QStringLiteral("scope")).toUrl();
    entry.themeColor = settings.value(QStringLiteral("theme")).toString();
    return entry;
}

QImage WebApps::icon(const QString &id) const
{
    return QImage(iconPath(id));
}

QString WebApps::iconPath(const QString &id) const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/apps/") + id + QStringLiteral(".png");
}

QString WebApps::idFor(const QUrl &startUrl)
{
    QString host = startUrl.host();
    if (host.startsWith(QLatin1String("www.")))
        host.remove(0, 4);
    host.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")), QStringLiteral("-"));
    const QByteArray hash = QCryptographicHash::hash(startUrl.toString(QUrl::RemoveFragment).toUtf8(),
                                                     QCryptographicHash::Sha1)
                                .toHex()
                                .left(6);
    return host.toLower() + QLatin1Char('-') + QString::fromLatin1(hash);
}

WebApp WebApps::fromManifest(const QByteArray &json, const QUrl &manifestUrl, const QUrl &pageUrl,
                             QUrl *iconUrl)
{
    const QJsonObject manifest = QJsonDocument::fromJson(json).object();
    WebApp app;
    app.name = manifest.value(QStringLiteral("name")).toString().trimmed();
    if (app.name.isEmpty())
        app.name = manifest.value(QStringLiteral("short_name")).toString().trimmed();

    // The start page must be on the page's own site; a manifest cannot send the
    // app somewhere else.
    const QUrl origin = pageUrl.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment);
    const QString start = manifest.value(QStringLiteral("start_url")).toString();
    app.startUrl = start.isEmpty() ? pageUrl : manifestUrl.resolved(QUrl(start));
    if (app.startUrl.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment) != origin)
        app.startUrl = pageUrl;
    app.startUrl.setFragment(QString());

    const QString scope = manifest.value(QStringLiteral("scope")).toString();
    app.scope = scope.isEmpty() ? QUrl() : manifestUrl.resolved(QUrl(scope));
    if (!app.scope.isValid()
        || app.scope.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment) != origin) {
        // The default scope: the start page's folder.
        app.scope = app.startUrl.adjusted(QUrl::RemoveFilename | QUrl::RemoveQuery | QUrl::RemoveFragment);
    }
    app.themeColor = manifest.value(QStringLiteral("theme_color")).toString();
    app.id = idFor(app.startUrl);

    if (iconUrl) {
        int best = -1;
        for (const QJsonValue &value : manifest.value(QStringLiteral("icons")).toArray()) {
            const QJsonObject icon = value.toObject();
            const int score = iconScore(icon);
            if (score > best) {
                best = score;
                *iconUrl = manifestUrl.resolved(QUrl(icon.value(QStringLiteral("src")).toString()));
            }
        }
    }
    return app;
}

QString WebApps::launcherProgram()
{
    return launcherCommand().join(QLatin1Char(' '));
}

QString WebApps::desktopEntry(const WebApp &app, const QString &program, const QString &iconPath)
{
    QStringList exec;
    for (const QString &argument : launcherCommand())
        exec.append(execArgument(argument));
    if (!program.isEmpty())
        exec = {execArgument(program)};
    exec.append(QStringLiteral("--app=") + app.id);

    QString name = app.name;
    name.replace(QLatin1Char('\n'), QLatin1Char(' '));
    QString entry = QStringLiteral("[Desktop Entry]\n"
                                   "Type=Application\n"
                                   "Name=%1\n"
                                   "Comment=%2\n"
                                   "Exec=%3\n"
                                   "Terminal=false\n"
                                   "Categories=Network;\n"
                                   "StartupNotify=true\n")
                        .arg(name, app.startUrl.host(), exec.join(QLatin1Char(' ')));
    if (!iconPath.isEmpty())
        entry += QStringLiteral("Icon=%1\n").arg(iconPath);
    return entry;
}

bool WebApps::install(WebApp app, const QImage &icon, QString *error)
{
    if (app.id.isEmpty())
        app.id = idFor(app.startUrl);
    if (app.name.trimmed().isEmpty())
        app.name = app.startUrl.host();
    if (!app.scope.isValid())
        app.scope = app.startUrl.adjusted(QUrl::RemoveFilename | QUrl::RemoveQuery | QUrl::RemoveFragment);

    // A reinstall replaces the earlier launcher, which may carry the old name.
    const WebApp previous = this->app(app.id);
    if (previous.isValid())
        removeLauncher(previous);

    QSettings settings;
    settings.beginGroup(kGroup + QLatin1Char('/') + app.id);
    settings.setValue(QStringLiteral("name"), app.name);
    settings.setValue(QStringLiteral("start"), app.startUrl);
    settings.setValue(QStringLiteral("scope"), app.scope);
    settings.setValue(QStringLiteral("theme"), app.themeColor);
    settings.endGroup();

    if (!icon.isNull()) {
        QDir().mkpath(QFileInfo(iconPath(app.id)).absolutePath());
        icon.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(iconPath(app.id), "PNG");
    }
    const bool added = addLauncher(app, icon, error);
    emit changed();
    return added;
}

void WebApps::remove(const QString &id)
{
    const WebApp entry = app(id);
    if (entry.isValid())
        removeLauncher(entry);
    QFile::remove(iconPath(id));
    QSettings settings;
    settings.remove(kGroup + QLatin1Char('/') + id);
    emit changed();
}

bool WebApps::addLauncher(const WebApp &app, const QImage &icon, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    QSettings settings;
    const QString launcherKey = kGroup + QLatin1Char('/') + app.id + QStringLiteral("/launcher");
#if defined(Q_OS_WIN)
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QDir().mkpath(folder);
    QString iconFile;
    if (!icon.isNull()) {
        iconFile = QFileInfo(iconPath(app.id)).absolutePath() + QLatin1Char('/') + app.id + QStringLiteral(".ico");
        if (!icon.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(iconFile, "ICO"))
            iconFile.clear();
    }
    const QStringList command = launcherCommand();
    QStringList arguments;
    for (const QString &argument : command.mid(1))
        arguments.append(windowsArgument(argument));
    arguments.append(QStringLiteral("--app=") + app.id);
    const QString link = folder + QLatin1Char('/') + fileName(app) + QStringLiteral(".lnk");
    if (!writeShortcut(link, command.constFirst(), arguments.join(QLatin1Char(' ')),
                       iconFile.isEmpty() ? command.constFirst() : iconFile, app.startUrl.host()))
        return fail(tr("Could not add it to the Start menu."));
    settings.setValue(launcherKey, link);
    return true;
#elif defined(Q_OS_MACOS)
    const QString bundle = QDir::homePath() + QStringLiteral("/Applications/") + fileName(app)
        + QStringLiteral(".app");
    const QString contents = bundle + QStringLiteral("/Contents");
    if (!QDir().mkpath(contents + QStringLiteral("/MacOS")) || !QDir().mkpath(contents + QStringLiteral("/Resources")))
        return fail(tr("Could not create %1.").arg(bundle));
    QStringList command;
    for (const QString &argument : launcherCommand())
        command.append(shellQuote(argument));
    command.append(shellQuote(QStringLiteral("--app=") + app.id));
    QFile script(contents + QStringLiteral("/MacOS/launcher"));
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(tr("Could not create %1.").arg(bundle));
    script.write(QStringLiteral("#!/bin/sh\nexec %1\n").arg(command.join(QLatin1Char(' '))).toUtf8());
    script.close();
    script.setPermissions(script.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup
                          | QFileDevice::ExeOther);
    const bool hasIcon = !icon.isNull()
        && icon.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation)
               .save(contents + QStringLiteral("/Resources/icon.icns"), "ICNS");
    QFile plist(contents + QStringLiteral("/Info.plist"));
    if (!plist.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(tr("Could not create %1.").arg(bundle));
    QString name = app.name.toHtmlEscaped();
    plist.write(QStringLiteral(
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                    "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                    "<plist version=\"1.0\"><dict>\n"
                    "<key>CFBundleName</key><string>%1</string>\n"
                    "<key>CFBundleDisplayName</key><string>%1</string>\n"
                    "<key>CFBundleIdentifier</key><string>org.spacepenguin.app.%2</string>\n"
                    "<key>CFBundleExecutable</key><string>launcher</string>\n"
                    "<key>CFBundlePackageType</key><string>APPL</string>\n"
                    "%3"
                    "</dict></plist>\n")
                    .arg(name, app.id,
                         hasIcon ? QStringLiteral("<key>CFBundleIconFile</key><string>icon</string>\n")
                                 : QString())
                    .toUtf8());
    plist.close();
    settings.setValue(launcherKey, bundle);
    return true;
#else
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QDir().mkpath(folder);
    const QString path = folder + QStringLiteral("/spacepenguin-app-") + app.id + QStringLiteral(".desktop");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(tr("Could not write %1.").arg(path));
    file.write(desktopEntry(app, QString(), icon.isNull() ? QString() : iconPath(app.id)).toUtf8());
    if (!file.commit())
        return fail(tr("Could not write %1.").arg(path));
    QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::ExeOwner);
    settings.setValue(launcherKey, path);
    return true;
#endif
}

void WebApps::removeLauncher(const WebApp &app)
{
    QSettings settings;
    const QString launcher =
        settings.value(kGroup + QLatin1Char('/') + app.id + QStringLiteral("/launcher")).toString();
    if (launcher.isEmpty())
        return;
    if (QFileInfo(launcher).isDir()) {
        // A macOS .app bundle, and only one this class made.
        if (launcher.endsWith(QLatin1String(".app"))
            && QFileInfo::exists(launcher + QStringLiteral("/Contents/MacOS/launcher")))
            QDir(launcher).removeRecursively();
    } else {
        QFile::remove(launcher);
    }
#ifdef Q_OS_WIN
    QFile::remove(QFileInfo(iconPath(app.id)).absolutePath() + QLatin1Char('/') + app.id + QStringLiteral(".ico"));
#endif
}

}

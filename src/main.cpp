#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QWebEngineProfile>
#include <QWebEngineUrlScheme>

#include "browserwindow.h"
#include "icons.h"
#include "profiles.h"
#include "theme.h"

#ifndef SPACEPENGUIN_VERSION
#    define SPACEPENGUIN_VERSION "0.0.0"
#endif

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // The start page scheme must be known to Chromium before the first profile exists.
    QWebEngineUrlScheme startScheme(QByteArrayLiteral("sp"));
    startScheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    startScheme.setFlags(QWebEngineUrlScheme::SecureScheme);
    QWebEngineUrlScheme::registerScheme(startScheme);

    QApplication app(argc, argv);
    // Fusion draws the same everywhere, which the chrome stylesheet relies on.
    QApplication::setStyle(spacepenguin::Theme::createStyle());
    QApplication::setWindowIcon(spacepenguin::Icons::appIcon());
    QCoreApplication::setOrganizationName(QStringLiteral("SpacePenguin"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("spacepenguin.invalid"));
    QCoreApplication::setApplicationName(QStringLiteral("SpacePenguin"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SPACEPENGUIN_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("A secure, lightweight, and usable browser written in Qt."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption privateModeOption(
        QStringList{QStringLiteral("private")},
        QStringLiteral("Do not write history, cookies, or cache to disk for this session."));
    parser.addOption(privateModeOption);

    const QCommandLineOption userDataOption(
        QStringList{QStringLiteral("user-data-dir")},
        QStringLiteral("Store persistent data in <dir> instead of the default location."),
        QStringLiteral("dir"));
    parser.addOption(userDataOption);

    const QCommandLineOption filterListOption(
        QStringList{QStringLiteral("filter-list")},
        QStringLiteral("Load ad blocking rules from <file>."),
        QStringLiteral("file"));
    parser.addOption(filterListOption);

    parser.addPositionalArgument(QStringLiteral("urls"),
                                 QStringLiteral("Addresses or searches to open in new tabs."),
                                 QStringLiteral("[urls...]"));

    parser.process(app);

    spacepenguin::ProfileServices services = spacepenguin::createProfileServices(
        &app, parser.isSet(privateModeOption), parser.value(filterListOption));

    if (parser.isSet(userDataOption)) {
        const QString dataDirectory = QDir(parser.value(userDataOption)).absolutePath();
        QDir().mkpath(dataDirectory);
        services.profile->setPersistentStoragePath(dataDirectory);
        services.profile->setCachePath(dataDirectory + QStringLiteral("/cache"));
    }

    spacepenguin::BrowserWindow window(std::move(services));
    window.show();

    const QStringList urls = parser.positionalArguments();
    for (const QString &url : urls)
        window.openInNewTab(url);
    return app.exec();
}
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QWebEngineProfile>

#include "browserwindow.h"
#include "profiles.h"

#ifndef SPACEPENGUIN_VERSION
#    define SPACEPENGUIN_VERSION "0.0.0"
#endif

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
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

    const     QCommandLineOption userDataOption(
        QStringList{QStringLiteral("user-data-dir")},
        QStringLiteral("Store persistent data in <dir> instead of the default location."),
        QStringLiteral("dir"));
    parser.addOption(userDataOption);

    const QCommandLineOption filterListOption(
        QStringList{QStringLiteral("filter-list")},
        QStringLiteral("Load ad blocking rules from <file>."),
        QStringLiteral("file"));
    parser.addOption(filterListOption);

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
    return app.exec();
}
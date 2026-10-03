#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <QWebEngineProfile>

#include "browserwindow.h"
#include "startpageschemehandler.h"

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

    const QCommandLineOption userDataOption(
        QStringList{QStringLiteral("user-data-dir")},
        QStringLiteral("Store persistent data in <dir> instead of the default location."),
        QStringLiteral("dir"));
    parser.addOption(userDataOption);

    parser.process(app);

    const QString dataDirectory = parser.isSet(userDataOption)
        ? QDir(parser.value(userDataOption)).absolutePath()
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    QWebEngineProfile *profile = nullptr;
    if (parser.isSet(privateModeOption)) {
        profile = new QWebEngineProfile(&app);
    } else {
        QDir().mkpath(dataDirectory);
        profile = new QWebEngineProfile(QStringLiteral("default"), &app);
        profile->setPersistentStoragePath(dataDirectory);
        profile->setCachePath(dataDirectory + QStringLiteral("/cache"));
    }

    profile->installUrlSchemeHandler(QByteArrayLiteral("sp"),
                                    new spacepenguin::StartPageSchemeHandler(profile));

    spacepenguin::BrowserWindow window(profile);
    window.show();
    return app.exec();
}
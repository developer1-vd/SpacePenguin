#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtWebView>

#ifndef SPACEPENGUIN_VERSION
#    define SPACEPENGUIN_VERSION "0.0.0"
#endif

int main(int argc, char *argv[])
{
    // Must come before the application object: it picks the platform's web view.
    QtWebView::initialize();

    QGuiApplication app(argc, argv);
    // The same names as the desktop app, so settings live in the same place.
    QCoreApplication::setOrganizationName(QStringLiteral("SpacePenguin"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("spacepenguin.invalid"));
    QCoreApplication::setApplicationName(QStringLiteral("SpacePenguin"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SPACEPENGUIN_VERSION));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("SpacePenguin", "Main");
    return app.exec();
}

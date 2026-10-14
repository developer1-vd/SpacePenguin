#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileOpenEvent>
#include <QWebEngineProfile>
#include <QWebEngineUrlScheme>

#include "browserwindow.h"
#include "icons.h"
#include "profiles.h"
#include "singleinstance.h"
#include "theme.h"

#include <functional>

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
    // Lets Wayland compositors and Linux docks match windows to the installed
    // launcher entry and its icon.
    QGuiApplication::setDesktopFileName(QStringLiteral("org.spacepenguin.SpacePenguin"));
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

    const bool isPrivate = parser.isSet(privateModeOption);
    const QStringList urls = parser.positionalArguments();

    // One process per profile: a second launch hands its pages to the first one.
    const QString dataDirectory = parser.isSet(userDataOption)
        ? QDir(parser.value(userDataOption)).absolutePath()
        : spacepenguin::defaultDataDirectory();
    spacepenguin::SingleInstance instance(dataDirectory);
    QStringList handOff = urls;
    if (isPrivate)
        handOff.prepend(QStringLiteral("--private"));
    if (instance.sendToRunningInstance(handOff))
        return 0;

    spacepenguin::ProfileServices services = spacepenguin::createProfileServices(
        &app, isPrivate, parser.value(filterListOption));

    if (parser.isSet(userDataOption)) {
        QDir().mkpath(dataDirectory);
        services.profile->setPersistentStoragePath(dataDirectory);
        services.profile->setCachePath(dataDirectory + QStringLiteral("/cache"));
    }

    spacepenguin::BrowserWindow window(std::move(services));
    window.show();

    for (const QString &url : urls)
        window.openInNewTab(url);

    // Pages that arrive from outside go to the window in front, the way they do
    // in every other browser.
    const auto frontWindow = [&window]() -> spacepenguin::BrowserWindow * {
        if (auto *active = qobject_cast<spacepenguin::BrowserWindow *>(QApplication::activeWindow());
            active && !active->isPrivate()) {
            return active;
        }
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget *widget : widgets) {
            auto *candidate = qobject_cast<spacepenguin::BrowserWindow *>(widget);
            if (candidate && candidate->isVisible() && !candidate->isPrivate())
                return candidate;
        }
        return &window;
    };
    const auto openFromOutside = [&frontWindow](QStringList texts) {
        spacepenguin::BrowserWindow *target = frontWindow();
        if (texts.removeAll(QStringLiteral("--private")) > 0) {
            target = target->openWindow(true, texts);
        } else if (texts.isEmpty()) {
            target->openInNewTab(QString());
        } else {
            for (const QString &text : std::as_const(texts))
                target->openInNewTab(text);
        }
        if (target->isMinimized())
            target->showNormal();
        target->show();
        target->raise();
        target->activateWindow();
        QApplication::alert(target);
    };

    if (!isPrivate) {
        QObject::connect(&instance, &spacepenguin::SingleInstance::argumentsReceived, &app,
                         openFromOutside);
        instance.listen();
    }

    // macOS delivers links and files opened from Finder, or sent to the default
    // browser, as events rather than command-line arguments.
    class FileOpenFilter : public QObject
    {
    public:
        explicit FileOpenFilter(std::function<void(QStringList)> open, QObject *parent)
            : QObject(parent), m_open(std::move(open)) {}

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (event->type() == QEvent::FileOpen) {
                const auto *open = static_cast<QFileOpenEvent *>(event);
                m_open({open->url().isLocalFile() ? open->file() : open->url().toString()});
                return true;
            }
            return QObject::eventFilter(watched, event);
        }

    private:
        std::function<void(QStringList)> m_open;
    };
    app.installEventFilter(new FileOpenFilter(openFromOutside, &app));

    return app.exec();
}
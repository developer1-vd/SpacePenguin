#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileOpenEvent>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QTimer>
#include <QWebEngineProfile>

#include <algorithm>
#include <QWebEngineUrlScheme>

#include "appwindow.h"
#include "browserwindow.h"
#include "classroom.h"
#include "devicetuning.h"
#include "icons.h"
#include "preferences.h"
#include "profiles.h"
#include "roles.h"
#include "sandbox.h"
#include "singleinstance.h"
#include "startuptrace.h"
#include "systemscheme.h"
#include "tor.h"
#include "theme.h"

#include <functional>

#ifndef SPACEPENGUIN_VERSION
#    define SPACEPENGUIN_VERSION "0.0.0"
#endif

int main(int argc, char *argv[])
{
    spacepenguin::StartupTrace::mark("process started");
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Before Chromium starts: fall back cleanly where its sandbox cannot run
    // (Ubuntu 23.10+ without an AppArmor profile) instead of aborting.
    spacepenguin::Sandbox::prepare();
    // Small machines, school Chromebooks above all: fewer renderer processes and,
    // where Linux apps get no GPU, software drawing instead of black pages.
    spacepenguin::DeviceTuning::prepare();

    // The start page scheme must be known to Chromium before the first profile exists.
    QWebEngineUrlScheme startScheme(QByteArrayLiteral("sp"));
    startScheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    // The store page asks sp://store/api/… for its data with fetch().
    startScheme.setFlags(QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::FetchApiAllowed);
    QWebEngineUrlScheme::registerScheme(startScheme);

    spacepenguin::StartupTrace::mark("sandbox and device checks done");

    // Qt reports that the desktop portal already knows this app when it was started
    // from a terminal that registered itself; nothing is wrong. QT_LOGGING_RULES
    // still overrides this.
    QLoggingCategory::setFilterRules(QStringLiteral("qt.qpa.services.warning=false"));
    QApplication app(argc, argv);
    spacepenguin::StartupTrace::mark("Qt application created");
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

    const QCommandLineOption appOption(
        QStringList{QStringLiteral("app")},
        QStringLiteral("Open the installed web app <id> in its own window (see Install as App)."),
        QStringLiteral("id"));
    parser.addOption(appOption);

    const QCommandLineOption torOption(
        QStringList{QStringLiteral("tor")},
        QStringLiteral("Open a private window that goes through Tor (and opens .onion sites)."));
    parser.addOption(torOption);

    parser.addPositionalArgument(QStringLiteral("urls"),
                                 QStringLiteral("Addresses or searches to open in new tabs."),
                                 QStringLiteral("[urls...]"));

    parser.process(app);

    const bool torMode = parser.isSet(torOption);
    const bool isPrivate = parser.isSet(privateModeOption) || torMode;
    const QStringList urls = parser.positionalArguments();
    const QString appId = parser.value(appOption);

    // One process per profile: a second launch hands its pages to the first one.
    const QString dataDirectory = parser.isSet(userDataOption)
        ? QDir(parser.value(userDataOption)).absolutePath()
        : spacepenguin::defaultDataDirectory();
    spacepenguin::SingleInstance instance(dataDirectory);
    QStringList handOff = urls;
    if (isPrivate)
        handOff.prepend(QStringLiteral("--private"));
    if (!appId.isEmpty())
        handOff = {QStringLiteral("--app=") + appId};
    // A Tor window is a process of its own, whatever else is open (see tor.h).
    if (!torMode && instance.sendToRunningInstance(handOff))
        return 0;
    if (torMode) {
        QString error;
        if (!spacepenguin::Tor::start(&error)) {
            QMessageBox::warning(nullptr, QStringLiteral("SpacePenguin"), error);
            return 1;
        }
    }

    spacepenguin::StartupTrace::mark("single-instance check done");
    // Gamer mode's Chromium switches, read once: they take effect from this start.
    {
        spacepenguin::Preferences *preferences = spacepenguin::Preferences::instance();
        QStringList flags = qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS").split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QStringList gamer = preferences->hasRole(spacepenguin::Roles::key(spacepenguin::Roles::Role::Gamer))
            ? spacepenguin::Roles::gamerFlags(preferences->gpuBoost(), preferences->uncapFrameRate(),
                                              preferences->showFpsCounter(),
                                              flags.contains(QStringLiteral("--disable-gpu")))
            : QStringList();
        qApp->setProperty("spacepenguin.gamerFlags", gamer);
        if (!gamer.isEmpty())
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", (flags + gamer).join(QLatin1Char(' ')).toLocal8Bit());
    }
    // Teacher and student mode talk over the school network; the Tor process never does.
    if (!torMode)
        spacepenguin::Classroom::instance()->start();
    spacepenguin::watchSystemScheme();
    spacepenguin::ProfileServices services = spacepenguin::createProfileServices(
        &app, isPrivate, parser.value(filterListOption));
    spacepenguin::StartupTrace::mark("browser profile, history and filters loaded");

    if (parser.isSet(userDataOption)) {
        QDir().mkpath(dataDirectory);
        services.profile->setPersistentStoragePath(dataDirectory);
        services.profile->setCachePath(dataDirectory + QStringLiteral("/cache"));
    }

    // An installed app on its own opens only its window; the browser's comes when
    // a page leaves the app, or when the browser itself is started.
    spacepenguin::BrowserWindow::registerServices(services);
    QWebEngineProfile *appProfile = services.profile;
    const auto tabForOutside = [](bool background) {
        return spacepenguin::BrowserWindow::tabForOutside(background);
    };
    if (appId.isEmpty() || isPrivate
        || !spacepenguin::AppWindow::open(appId, appProfile, tabForOutside)) {
        auto *window = new spacepenguin::BrowserWindow(std::move(services));
        window->setAttribute(Qt::WA_DeleteOnClose);
        spacepenguin::StartupTrace::mark("window built");
        window->show();
        QTimer::singleShot(0, [] { spacepenguin::StartupTrace::mark("window shown"); });
        for (const QString &url : urls)
            window->openInNewTab(url);
    }

    // Pages that arrive from outside go to the window in front, the way they do
    // in every other browser.
    const auto frontWindow = [] { return spacepenguin::BrowserWindow::frontWindow(); };
    const auto openFromOutside = [&frontWindow, appProfile, tabForOutside](QStringList texts) {
        // An installed app's launcher, while SpacePenguin is already running.
        for (const QString &text : std::as_const(texts)) {
            if (text.startsWith(QLatin1String("--app="))
                && spacepenguin::AppWindow::open(text.mid(6), appProfile, tabForOutside))
                return;
        }
        texts.erase(std::remove_if(texts.begin(), texts.end(),
                                   [](const QString &text) { return text.startsWith(QLatin1String("--app=")); }),
                    texts.end());
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
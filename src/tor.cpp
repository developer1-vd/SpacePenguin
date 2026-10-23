#include "tor.h"

#include "classroom.h"
#include "webapps.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>

#include <cstring>

namespace spacepenguin::Tor {
namespace {

bool s_active = false;

const QString kDuckDuckGo = QStringLiteral("https://duckduckgo.com/?q={query}");
const QString kDuckDuckGoOnion =
    QStringLiteral("https://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion/?q={query}");

/** The tor program: on the PATH, or where Homebrew and the usual packages put it. */
QString torProgram()
{
    QString program = QStandardPaths::findExecutable(QStringLiteral("tor"));
    if (!program.isEmpty())
        return program;
    for (const QString &candidate : {QStringLiteral("/opt/homebrew/bin/tor"), QStringLiteral("/usr/local/bin/tor"),
                                     QStringLiteral("/usr/bin/tor"), QStringLiteral("/usr/sbin/tor")}) {
        if (QFileInfo(candidate).isExecutable())
            return candidate;
    }
    return QString();
}

/** A port nothing listens on, for a Tor of our own. */
quint16 freePort()
{
    for (quint16 port = 9052; port < 9080; ++port) {
        QTcpServer probe;
        if (probe.listen(QHostAddress::LocalHost, port))
            return port;
    }
    return 0;
}

/** Starts tor on @p port and waits, with a progress dialog, until it has a circuit. */
bool startOwnTor(const QString &program, quint16 port, QString *error)
{
    const QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/tor");
    QDir().mkpath(dataDirectory);
    // Tor refuses a data folder others can read.
    QFile::setPermissions(dataDirectory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    // Lives as long as this process; Qt stops it on the way out.
    auto *tor = new QProcess(qApp);
    tor->setProgram(program);
    tor->setArguments({QStringLiteral("--SocksPort"), QStringLiteral("127.0.0.1:%1").arg(port),
                       QStringLiteral("--DataDirectory"), dataDirectory,
                       QStringLiteral("--ClientOnly"), QStringLiteral("1"),
                       QStringLiteral("--Log"), QStringLiteral("notice stdout")});
    tor->setProcessChannelMode(QProcess::MergedChannels);
    tor->start();
    if (!tor->waitForStarted(10000)) {
        *error = QCoreApplication::translate("Tor", "Tor could not be started (%1).").arg(tor->errorString());
        return false;
    }

    QProgressDialog progress(QCoreApplication::translate("Tor", "Connecting to the Tor network…"),
                             QCoreApplication::translate("Tor", "Cancel"), 0, 100);
    progress.setWindowTitle(QStringLiteral("SpacePenguin"));
    progress.setMinimumDuration(0);
    progress.setValue(0);

    static const QRegularExpression bootstrapped(QStringLiteral("Bootstrapped (\\d+)%"));
    QElapsedTimer timer;
    timer.start();
    QByteArray log;
    while (timer.elapsed() < 180000) {
        if (progress.wasCanceled()) {
            *error = QCoreApplication::translate("Tor", "Connecting to Tor was cancelled.");
            return false;
        }
        if (tor->state() == QProcess::NotRunning) {
            *error = QCoreApplication::translate("Tor", "Tor stopped while connecting:\n%1")
                         .arg(QString::fromLocal8Bit(log.right(600)));
            return false;
        }
        tor->waitForReadyRead(200);
        log += tor->readAll();
        int percent = 0;
        auto matches = bootstrapped.globalMatch(QString::fromLocal8Bit(log));
        while (matches.hasNext())
            percent = matches.next().captured(1).toInt();
        progress.setValue(qMin(percent, 99));
        QCoreApplication::processEvents();
        if (percent >= 100)
            return true;
    }
    *error = QCoreApplication::translate("Tor", "Tor could not reach its network in three minutes.");
    return false;
}

} // namespace

bool isActive()
{
    return s_active;
}

bool requested(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tor") == 0)
            return true;
    }
    return false;
}

bool isTorPort(quint16 port)
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    if (!socket.waitForConnected(400))
        return false;
    socket.write("GET / HTTP/1.0\r\n\r\n");
    socket.waitForBytesWritten(400);
    QByteArray answer;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1500 && socket.waitForReadyRead(300))
        answer += socket.readAll();
    return answer.contains("Tor is not an HTTP Proxy");
}

QString installHint()
{
#if defined(Q_OS_WIN)
    return QCoreApplication::translate("Tor", "Open Tor Browser (from torproject.org) and leave it running: "
                                              "SpacePenguin uses its connection to the Tor network.");
#elif defined(Q_OS_MACOS)
    return QCoreApplication::translate("Tor", "Install Tor with Homebrew (brew install tor), or open Tor "
                                              "Browser and leave it running.");
#else
    return QCoreApplication::translate("Tor", "Install Tor with your package manager (for example: sudo apt "
                                              "install tor), or open Tor Browser and leave it running.");
#endif
}

bool start(QString *error)
{
    if (Classroom::instance()->locked()) {
        if (error)
            *error = QCoreApplication::translate("Tor", "Tor windows are off while you're in a class.");
        return false;
    }
    QString message;
    // The port can be given; otherwise the tor service's, then Tor Browser's.
    quint16 port = 0;
    const int chosen = qEnvironmentVariableIntValue("SPACEPENGUIN_TOR_PORT");
    for (const int candidate : {chosen, 9050, 9150}) {
        if (candidate > 0 && candidate < 65536 && isTorPort(quint16(candidate))) {
            port = quint16(candidate);
            break;
        }
    }
    if (!port) {
        const QString program = torProgram();
        if (program.isEmpty()) {
            message = QCoreApplication::translate("Tor", "Tor windows need Tor, and there is none on this "
                                                         "computer.\n\n%1")
                          .arg(installHint());
        } else if (const quint16 own = freePort(); !own) {
            message = QCoreApplication::translate("Tor", "No free port for Tor.");
        } else if (startOwnTor(program, own, &message)) {
            port = own;
        }
    }
    if (!port) {
        if (error)
            *error = message;
        return false;
    }

    // Everything this process loads goes through Tor, which resolves the names too;
    // WebRTC may not reach out on its own.
    QStringList flags = qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS").split(QLatin1Char(' '), Qt::SkipEmptyParts);
    flags << QStringLiteral("--proxy-server=socks5://127.0.0.1:%1").arg(port)
          << QStringLiteral("--force-webrtc-ip-handling-policy=disable_non_proxied_udp");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags.join(QLatin1Char(' ')).toLocal8Bit());
    s_active = true;
    return true;
}

bool isOnion(const QUrl &url)
{
    return url.host().endsWith(QLatin1String(".onion"), Qt::CaseInsensitive);
}

bool openWindow(const QStringList &urls, QString *error)
{
    // In a class, Tor would get around its blocks.
    if (Classroom::instance()->locked()) {
        if (error)
            *error = QCoreApplication::translate("Tor", "Tor windows are off while you're in a class.");
        return false;
    }
    // Inside a Flatpak this process starts its own binary; launchers outside it use
    // "flatpak run".
    const QStringList command = qEnvironmentVariableIsSet("FLATPAK_ID")
        ? QStringList{QCoreApplication::applicationFilePath()}
        : WebApps::launcherCommand();
    QStringList arguments = command.mid(1);
    arguments << QStringLiteral("--tor") << urls;
    if (QProcess::startDetached(command.constFirst(), arguments))
        return true;
    if (error)
        *error = QCoreApplication::translate("Tor", "The Tor window could not be opened.");
    return false;
}

QString searchTemplate(const QString &clearnetTemplate)
{
    return clearnetTemplate == kDuckDuckGo ? kDuckDuckGoOnion : clearnetTemplate;
}

}

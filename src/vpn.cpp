#include "vpn.h"

#include "tor.h"

#include <QCoreApplication>
#include <QNetworkProxy>
#include <QProcess>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QSettings>
#include <QThread>

#include <algorithm>

#ifdef Q_OS_WIN
#    include <windows.h>
#else
#    include <signal.h>
#endif

namespace spacepenguin::Vpn {
namespace {

Mode s_active = Mode::Off;
QString s_description;

const QString kMode = QStringLiteral("vpn/mode");
const QString kType = QStringLiteral("vpn/proxyType");
const QString kHost = QStringLiteral("vpn/proxyHost");
const QString kPort = QStringLiteral("vpn/proxyPort");

void addFlags(const QString &proxy)
{
    QStringList flags = qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS").split(QLatin1Char(' '), Qt::SkipEmptyParts);
    // Whatever proxy was there gives way to the VPN's.
    flags.erase(std::remove_if(flags.begin(), flags.end(),
                               [](const QString &flag) { return flag.startsWith(QLatin1String("--proxy-server")); }),
                flags.end());
    // Names are resolved by the proxy (SOCKS5 resolves remotely), and WebRTC may
    // not reach out around it.
    flags << QStringLiteral("--proxy-server=%1").arg(proxy)
          << QStringLiteral("--force-webrtc-ip-handling-policy=disable_non_proxied_udp");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags.join(QLatin1Char(' ')).toLocal8Bit());
}

} // namespace

QString modeKey(Mode mode)
{
    switch (mode) {
    case Mode::Off: return QStringLiteral("off");
    case Mode::Tor: return QStringLiteral("tor");
    case Mode::Proxy: return QStringLiteral("proxy");
    }
    return QStringLiteral("off");
}

Mode modeFromKey(const QString &key)
{
    if (key == QLatin1String("tor"))
        return Mode::Tor;
    if (key == QLatin1String("proxy"))
        return Mode::Proxy;
    return Mode::Off;
}

QString proxyAddress(const Server &server)
{
    // Qt's own requests can follow these two; others would slip past the VPN.
    static const QStringList types{QStringLiteral("socks5"), QStringLiteral("http")};
    // A host name or an IP address, nothing else: the address joins a command line.
    static const QRegularExpression hostName(QStringLiteral("^[A-Za-z0-9](?:[A-Za-z0-9.-]{0,251}[A-Za-z0-9])?$"));
    static const QRegularExpression ipv6(QStringLiteral("^\\[?[0-9A-Fa-f:.]{2,45}\\]?$"));
    const QString type = server.type.trimmed().toLower();
    QString host = server.host.trimmed();
    if (!types.contains(type) || server.port < 1 || server.port > 65535)
        return QString();
    if (hostName.match(host).hasMatch()) {
        // fine
    } else if (host.contains(QLatin1Char(':')) && ipv6.match(host).hasMatch()) {
        host.remove(QLatin1Char('[')).remove(QLatin1Char(']'));
        host = QStringLiteral("[%1]").arg(host);
    } else {
        return QString();
    }
    return QStringLiteral("%1://%2:%3").arg(type, host).arg(server.port);
}

Mode chosenMode()
{
    return modeFromKey(QSettings().value(kMode).toString());
}

Server chosenServer()
{
    const QSettings settings;
    return {settings.value(kType, QStringLiteral("socks5")).toString(), settings.value(kHost).toString(),
            settings.value(kPort, 1080).toInt()};
}

bool apply(QString *error)
{
    const Mode mode = chosenMode();
    if (mode == Mode::Off)
        return true;
    if (mode == Mode::Tor) {
        const quint16 port = Tor::startForVpn(error);
        if (!port)
            return false;
        addFlags(QStringLiteral("socks5://127.0.0.1:%1").arg(port));
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::Socks5Proxy, QStringLiteral("127.0.0.1"), port));
        s_active = Mode::Tor;
        s_description = QStringLiteral("Tor");
        return true;
    }
    const Server server = chosenServer();
    const QString address = proxyAddress(server);
    if (address.isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("Vpn", "The VPN server in Settings › Privacy & Security is not "
                                                        "complete: it needs a type, a host and a port.");
        return false;
    }
    addFlags(address);
    // SpacePenguin's own requests (filter lists, updates, translation) go the same way.
    const bool socks = server.type.trimmed().toLower() == QLatin1String("socks5");
    QString host = server.host.trimmed();
    host.remove(QLatin1Char('[')).remove(QLatin1Char(']'));
    QNetworkProxy proxy(socks ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy, host, quint16(server.port));
    // Names are resolved by the proxy, as Chromium does.
    proxy.setCapabilities(proxy.capabilities() | QNetworkProxy::HostNameLookupCapability);
    QNetworkProxy::setApplicationProxy(proxy);
    s_active = Mode::Proxy;
    s_description = QStringLiteral("%1:%2").arg(server.host.trimmed()).arg(server.port);
    return true;
}

Mode activeMode()
{
    return s_active;
}

QString describeActive()
{
    return s_description;
}

void restart()
{
    // The new process waits for this one to be gone (it holds the profile and
    // the single-instance socket), then restores its tabs once, whatever the
    // start-up setting.
    QStringList arguments = QCoreApplication::arguments().mid(1);
    arguments.removeIf([](const QString &argument) {
        return !argument.startsWith(QLatin1String("--")) || argument.startsWith(QLatin1String("--app"));
    });
    qputenv("SPACEPENGUIN_RESTART_OF", QByteArray::number(QCoreApplication::applicationPid()));
    QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
    qunsetenv("SPACEPENGUIN_RESTART_OF");
}

bool waitForPreviousProcess()
{
    const qint64 pid = qEnvironmentVariable("SPACEPENGUIN_RESTART_OF").toLongLong();
    qunsetenv("SPACEPENGUIN_RESTART_OF");
    if (pid <= 0)
        return false;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 20000) {
#ifdef Q_OS_WIN
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, DWORD(pid));
        if (!process)
            break;
        const DWORD result = WaitForSingleObject(process, 200);
        CloseHandle(process);
        if (result == WAIT_OBJECT_0)
            break;
#else
        if (kill(pid_t(pid), 0) != 0)
            break;
        QThread::msleep(100);
#endif
    }
    return true;
}

}

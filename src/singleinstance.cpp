#include "singleinstance.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QLocalServer>
#include <QLocalSocket>

namespace spacepenguin {
namespace {

constexpr int kTimeoutMs = 1000;

} // namespace

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
{
    // Socket names must be short and free of path separators on every platform.
    const QByteArray hash =
        QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    m_serverName = QStringLiteral("spacepenguin-") + QString::fromLatin1(hash);
}

bool SingleInstance::sendToRunningInstance(const QStringList &arguments)
{
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    if (!socket.waitForConnected(kTimeoutMs))
        return false;

    QByteArray message;
    QDataStream stream(&message, QIODevice::WriteOnly);
    stream << arguments;
    socket.write(message);
    if (!socket.waitForBytesWritten(kTimeoutMs))
        return false;
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState)
        socket.waitForDisconnected(kTimeoutMs);
    return true;
}

bool SingleInstance::listen()
{
    m_server = new QLocalServer(this);
    // Only this user may connect.
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_serverName)) {
        // A socket left behind by a crashed process (Unix) blocks the name.
        QLocalServer::removeServer(m_serverName);
        if (!m_server->listen(m_serverName))
            return false;
    }

    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readChannelFinished, this, [this, socket] {
                QDataStream stream(socket->readAll());
                QStringList arguments;
                stream >> arguments;
                if (stream.status() == QDataStream::Ok)
                    emit argumentsReceived(arguments);
            });
        }
    });
    return true;
}

}

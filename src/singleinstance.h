#pragma once

#include <QObject>
#include <QStringList>

class QLocalServer;

namespace spacepenguin {

/**
 * Keeps one browser process per profile. Chromium locks a profile to the process
 * that opened it, so when the desktop launches SpacePenguin again (a link clicked
 * elsewhere, a second launcher click) the new process hands its addresses to the
 * running one over a local socket (a named pipe on Windows) and exits.
 */
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    /** @p key identifies the profile, such as its data directory. */
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);

    /** True when a running instance accepted @p arguments; this process should exit. */
    bool sendToRunningInstance(const QStringList &arguments);

    /** Starts accepting hand-offs from later launches. */
    bool listen();

    QString serverName() const { return m_serverName; }

signals:
    /** A later launch asked for these addresses; empty means "just show a window". */
    void argumentsReceived(const QStringList &arguments);

private:
    QString m_serverName;
    QLocalServer *m_server = nullptr;
};

}

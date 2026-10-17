#pragma once

#include <QObject>
#include <QPointer>
#include <QUrl>

#include <memory>

class QSystemTrayIcon;
class QWebEngineNotification;

namespace spacepenguin {

/**
 * Shows web notifications from sites that were allowed to send them: as desktop
 * notifications through the system tray, or, where the desktop has no tray to
 * show them with, inside the browser window (shownInWindow). Clicking one tells
 * the page and brings its tab forward (clicked).
 */
class Notifications : public QObject
{
    Q_OBJECT

public:
    static Notifications *instance();

    void present(std::unique_ptr<QWebEngineNotification> notification);

signals:
    /** The person clicked a notification from @p origin. */
    void clicked(const QUrl &origin);
    /** No desktop notifications here: a window should show this one itself. */
    void shownInWindow(const QString &title, const QString &message, const QUrl &origin);

private:
    explicit Notifications(QObject *parent = nullptr);

    QSystemTrayIcon *m_tray = nullptr;
    QPointer<QWebEngineNotification> m_latest;
};

}

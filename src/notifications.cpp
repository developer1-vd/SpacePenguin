#include "notifications.h"

#include "icons.h"

#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWebEngineNotification>

namespace spacepenguin {
namespace {

// How long the tray icon stays after a notification, so the desktop can show it.
constexpr int kTrayLingerMs = 15000;

} // namespace

Notifications *Notifications::instance()
{
    static auto *notifications = new Notifications(qApp);
    return notifications;
}

Notifications::Notifications(QObject *parent)
    : QObject(parent)
{
    if (QSystemTrayIcon::isSystemTrayAvailable() && QSystemTrayIcon::supportsMessages()) {
        m_tray = new QSystemTrayIcon(Icons::appIcon(), this);
        m_tray->setToolTip(QStringLiteral("SpacePenguin"));
        connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] {
            if (m_latest) {
                m_latest->click();
                emit clicked(m_latest->origin());
            }
        });
    }
}

void Notifications::present(std::unique_ptr<QWebEngineNotification> notification)
{
    QWebEngineNotification *shown = notification.release();
    shown->setParent(this);
    connect(shown, &QWebEngineNotification::closed, shown, &QObject::deleteLater);
    // A page that replaces a notification closes the old one; forget it after a while anyway.
    QTimer::singleShot(10 * 60 * 1000, shown, &QObject::deleteLater);

    const QString title = shown->title().isEmpty() ? shown->origin().host() : shown->title();
    if (m_tray) {
        // Desktops show tray messages only while the tray icon is there.
        m_tray->show();
        const QImage image = shown->icon();
        if (!image.isNull())
            m_tray->showMessage(title, shown->message(), QIcon(QPixmap::fromImage(image)), 8000);
        else
            m_tray->showMessage(title, shown->message(), QSystemTrayIcon::NoIcon, 8000);
        QTimer::singleShot(kTrayLingerMs, m_tray, [this, shown = QPointer(shown)] {
            if (shown == m_latest)
                m_tray->hide();
        });
    } else {
        emit shownInWindow(title, shown->message(), shown->origin());
    }
    m_latest = shown;
    shown->show();
}

}

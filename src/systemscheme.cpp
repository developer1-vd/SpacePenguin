#include "systemscheme.h"

#include "preferences.h"
#include "theme.h"

#ifdef SPACEPENGUIN_HAVE_DBUS
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QObject>
#endif

namespace spacepenguin {

#ifdef SPACEPENGUIN_HAVE_DBUS
namespace {

const char *const kService = "org.freedesktop.portal.Desktop";
const char *const kPath = "/org/freedesktop/portal/desktop";
const char *const kInterface = "org.freedesktop.portal.Settings";
const char *const kNamespace = "org.freedesktop.appearance";
const char *const kKey = "color-scheme";

// The portal's values: 0 no preference, 1 prefer dark, 2 prefer light.
Qt::ColorScheme schemeFromPortal(QVariant value)
{
    // Read() wraps the value in a variant twice, ReadOne() once.
    while (value.canConvert<QDBusVariant>() && value.userType() == qMetaTypeId<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    bool ok = false;
    const uint scheme = value.toUInt(&ok);
    if (!ok)
        return Qt::ColorScheme::Unknown;
    if (scheme == 1)
        return Qt::ColorScheme::Dark;
    if (scheme == 2)
        return Qt::ColorScheme::Light;
    return Qt::ColorScheme::Unknown;
}

Qt::ColorScheme readScheme(const char *method)
{
    QDBusMessage call = QDBusMessage::createMethodCall(QLatin1String(kService), QLatin1String(kPath),
                                                       QLatin1String(kInterface), QLatin1String(method));
    call << QLatin1String(kNamespace) << QLatin1String(kKey);
    // A short wait: without a portal the browser starts as Qt sees the desktop.
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 500);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return Qt::ColorScheme::Unknown;
    return schemeFromPortal(reply.arguments().constFirst());
}

class PortalListener : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

public slots:
    void settingChanged(const QString &group, const QString &key, const QDBusVariant &value)
    {
        if (group != QLatin1String(kNamespace) || key != QLatin1String(kKey))
            return;
        Theme::setSystemScheme(schemeFromPortal(QVariant::fromValue(value)));
        // As when the theme is picked in the menu: open built-in pages follow.
        Preferences::instance()->notifyChanged();
    }
};

} // namespace
#endif

void watchSystemScheme()
{
#ifdef SPACEPENGUIN_HAVE_DBUS
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    Qt::ColorScheme scheme = readScheme("ReadOne");
    if (scheme == Qt::ColorScheme::Unknown)
        scheme = readScheme("Read"); // portals older than version 2
    Theme::setSystemScheme(scheme);

    auto *listener = new PortalListener(qApp);
    bus.connect(QLatin1String(kService), QLatin1String(kPath), QLatin1String(kInterface),
                QStringLiteral("SettingChanged"), listener,
                SLOT(settingChanged(QString, QString, QDBusVariant)));
#endif
}

}

#ifdef SPACEPENGUIN_HAVE_DBUS
#include "systemscheme.moc"
#endif

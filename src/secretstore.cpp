#include "secretstore.h"

#include <QCoreApplication>
#include <QtGlobal>

#if defined(Q_OS_MACOS)
#    include <CoreFoundation/CoreFoundation.h>
#    include <Security/Security.h>
#elif defined(SPACEPENGUIN_HAVE_DBUS)
#    include <QDBusArgument>
#    include <QDBusConnection>
#    include <QDBusConnectionInterface>
#    include <QDBusInterface>
#    include <QDBusMetaType>
#    include <QDBusObjectPath>
#    include <QDBusReply>
#    include <QDBusVariant>
#    include <QEventLoop>
#    include <QMap>
#    include <QTimer>
#endif

namespace spacepenguin {

#if defined(SPACEPENGUIN_HAVE_DBUS) && !defined(Q_OS_MACOS)
namespace {

// org.freedesktop.Secret's (oayays): a secret as it crosses the bus.
struct DBusSecret {
    QDBusObjectPath session;
    QByteArray parameters;
    QByteArray value;
    QString contentType;
};

}
#endif

}

#if defined(SPACEPENGUIN_HAVE_DBUS) && !defined(Q_OS_MACOS)
Q_DECLARE_METATYPE(spacepenguin::DBusSecret)

QDBusArgument &operator<<(QDBusArgument &argument, const spacepenguin::DBusSecret &secret)
{
    argument.beginStructure();
    argument << secret.session << secret.parameters << secret.value << secret.contentType;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, spacepenguin::DBusSecret &secret)
{
    argument.beginStructure();
    argument >> secret.session >> secret.parameters >> secret.value >> secret.contentType;
    argument.endStructure();
    return argument;
}
#endif

namespace spacepenguin::SecretStore {
namespace {

bool turnedOff()
{
    return qEnvironmentVariableIntValue("SPACEPENGUIN_NO_KEYRING") == 1;
}

#if defined(Q_OS_MACOS)

// ---- macOS: the login Keychain --------------------------------------------------

CFMutableDictionaryRef baseQuery(const QString &key)
{
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    CFStringRef account = key.toCFString();
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, CFSTR("SpacePenguin"));
    CFDictionarySetValue(query, kSecAttrAccount, account);
    CFRelease(account);
    return query;
}

#elif defined(SPACEPENGUIN_HAVE_DBUS)

// ---- Linux and the BSDs: the Secret Service over D-Bus ----------------------------

using Attributes = QMap<QString, QString>;

const QString kService = QStringLiteral("org.freedesktop.secrets");
const QString kServicePath = QStringLiteral("/org/freedesktop/secrets");
const QString kServiceInterface = QStringLiteral("org.freedesktop.Secret.Service");
const QString kCollectionInterface = QStringLiteral("org.freedesktop.Secret.Collection");
const QString kItemInterface = QStringLiteral("org.freedesktop.Secret.Item");
const QString kPromptInterface = QStringLiteral("org.freedesktop.Secret.Prompt");
constexpr int kTimeoutMs = 10 * 1000;
// Long enough to type the keyring's password when it asks.
constexpr int kPromptTimeoutMs = 2 * 60 * 1000;

void registerTypes()
{
    static const bool done = [] {
        qDBusRegisterMetaType<DBusSecret>();
        qDBusRegisterMetaType<Attributes>();
        qDBusRegisterMetaType<QMap<QDBusObjectPath, DBusSecret>>();
        return true;
    }();
    Q_UNUSED(done);
}

Attributes attributesFor(const QString &key)
{
    return {{QStringLiteral("xdg:schema"), QStringLiteral("org.spacepenguin.Secret")},
            {QStringLiteral("application"), QStringLiteral("SpacePenguin")},
            {QStringLiteral("key"), key}};
}

/** Waits for a prompt (the keyring's password dialog); true unless it was dismissed. */
class PromptWaiter : public QObject
{
    Q_OBJECT
public:
    bool run(const QDBusObjectPath &prompt)
    {
        if (prompt.path().isEmpty() || prompt.path() == QLatin1String("/"))
            return true;
        QDBusConnection bus = QDBusConnection::sessionBus();
        bus.connect(kService, prompt.path(), kPromptInterface, QStringLiteral("Completed"), this,
                    SLOT(completed(bool, QDBusVariant)));
        QDBusInterface(kService, prompt.path(), kPromptInterface, bus).call(QStringLiteral("Prompt"), QString());
        QTimer::singleShot(kPromptTimeoutMs, &m_loop, &QEventLoop::quit);
        m_loop.exec();
        bus.disconnect(kService, prompt.path(), kPromptInterface, QStringLiteral("Completed"), this,
                       SLOT(completed(bool, QDBusVariant)));
        return m_done && !m_dismissed;
    }

private slots:
    void completed(bool dismissed, const QDBusVariant &)
    {
        m_done = true;
        m_dismissed = dismissed;
        m_loop.quit();
    }

private:
    QEventLoop m_loop;
    bool m_done = false;
    bool m_dismissed = false;
};

class Connection
{
public:
    Connection()
        : m_bus(QDBusConnection::sessionBus())
        , m_service(kService, kServicePath, kServiceInterface, m_bus)
    {
        registerTypes();
        m_service.setTimeout(kTimeoutMs);
        // "plain": the secret crosses the user's own session bus unencrypted, as
        // with libsecret when no encrypted session is negotiated.
        const QDBusMessage reply = m_service.call(QStringLiteral("OpenSession"), QStringLiteral("plain"),
                                                  QVariant::fromValue(QDBusVariant(QString())));
        if (reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 2)
            m_session = qdbus_cast<QDBusObjectPath>(reply.arguments().at(1));
    }
    ~Connection()
    {
        if (isOpen())
            QDBusInterface(kService, m_session.path(), QStringLiteral("org.freedesktop.Secret.Session"), m_bus)
                .call(QStringLiteral("Close"));
    }

    bool isOpen() const { return !m_session.path().isEmpty() && m_session.path() != QLatin1String("/"); }

    /** The items stored under @p key, unlocked; false if they could not be unlocked. */
    bool find(const QString &key, QList<QDBusObjectPath> *items)
    {
        const QDBusMessage reply = m_service.call(QStringLiteral("SearchItems"), QVariant::fromValue(attributesFor(key)));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 2)
            return false;
        *items = qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().at(0));
        const QList<QDBusObjectPath> locked = qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().at(1));
        if (locked.isEmpty())
            return true;
        if (!unlock(locked))
            return false;
        *items += locked;
        return true;
    }

    bool unlock(const QList<QDBusObjectPath> &objects)
    {
        const QDBusMessage reply = m_service.call(QStringLiteral("Unlock"), QVariant::fromValue(objects));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 2)
            return false;
        const QList<QDBusObjectPath> unlocked = qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().at(0));
        if (unlocked.size() == objects.size())
            return true;
        PromptWaiter waiter;
        return waiter.run(qdbus_cast<QDBusObjectPath>(reply.arguments().at(1)));
    }

    bool secret(const QDBusObjectPath &item, QByteArray *data)
    {
        QDBusInterface itemInterface(kService, item.path(), kItemInterface, m_bus);
        itemInterface.setTimeout(kTimeoutMs);
        const QDBusMessage reply = itemInterface.call(QStringLiteral("GetSecret"), QVariant::fromValue(m_session));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
            return false;
        *data = qdbus_cast<DBusSecret>(reply.arguments().at(0)).value;
        return true;
    }

    /** The default collection ("login"), unlocked; empty if there is none. */
    QDBusObjectPath defaultCollection()
    {
        const QDBusReply<QDBusObjectPath> alias = m_service.call(QStringLiteral("ReadAlias"), QStringLiteral("default"));
        if (!alias.isValid() || alias.value().path() == QLatin1String("/"))
            return {};
        QDBusInterface collection(kService, alias.value().path(), kCollectionInterface, m_bus);
        if (collection.property("Locked").toBool() && !unlock({alias.value()}))
            return {};
        return alias.value();
    }

    bool create(const QDBusObjectPath &collectionPath, const QString &key, const QByteArray &data,
                const QString &label)
    {
        QDBusInterface collection(kService, collectionPath.path(), kCollectionInterface, m_bus);
        collection.setTimeout(kTimeoutMs);
        const QVariantMap properties{
            {QStringLiteral("org.freedesktop.Secret.Item.Label"), label},
            {QStringLiteral("org.freedesktop.Secret.Item.Attributes"), QVariant::fromValue(attributesFor(key))},
        };
        const DBusSecret secret{m_session, {}, data, QStringLiteral("application/octet-stream")};
        const QDBusMessage reply = collection.call(QStringLiteral("CreateItem"), properties,
                                                   QVariant::fromValue(secret), true);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 2)
            return false;
        const QDBusObjectPath item = qdbus_cast<QDBusObjectPath>(reply.arguments().at(0));
        if (item.path() != QLatin1String("/"))
            return true;
        PromptWaiter waiter;
        return waiter.run(qdbus_cast<QDBusObjectPath>(reply.arguments().at(1)));
    }

    bool deleteItem(const QDBusObjectPath &item)
    {
        QDBusInterface itemInterface(kService, item.path(), kItemInterface, m_bus);
        const QDBusMessage reply = itemInterface.call(QStringLiteral("Delete"));
        if (reply.type() != QDBusMessage::ReplyMessage)
            return false;
        PromptWaiter waiter;
        return reply.arguments().isEmpty() || waiter.run(qdbus_cast<QDBusObjectPath>(reply.arguments().at(0)));
    }

private:
    QDBusConnection m_bus;
    QDBusInterface m_service;
    QDBusObjectPath m_session;
};

#endif

} // namespace

bool isAvailable()
{
    if (turnedOff())
        return false;
#if defined(Q_OS_MACOS)
    return true;
#elif defined(SPACEPENGUIN_HAVE_DBUS)
    static const bool available = [] {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected() || !bus.interface())
            return false;
        // Running, or started on demand by D-Bus (as gnome-keyring is).
        if (!bus.interface()->isServiceRegistered(kService).value()
            && !bus.interface()->activatableServiceNames().value().contains(kService))
            return false;
        return Connection().isOpen();
    }();
    return available;
#else
    return false;
#endif
}

QString name()
{
    if (!isAvailable())
        return {};
#if defined(Q_OS_MACOS)
    return QCoreApplication::translate("SecretStore", "the macOS Keychain");
#else
    return QCoreApplication::translate("SecretStore", "your desktop's keyring");
#endif
}

Result read(const QString &key, QByteArray *data)
{
    if (!isAvailable())
        return Result::Unavailable;
#if defined(Q_OS_MACOS)
    CFMutableDictionaryRef query = baseQuery(key);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    if (status == errSecItemNotFound)
        return Result::NotFound;
    if (status != errSecSuccess || !result)
        return Result::Unavailable;
    *data = QByteArray::fromCFData(static_cast<CFDataRef>(result));
    CFRelease(result);
    return Result::Found;
#elif defined(SPACEPENGUIN_HAVE_DBUS)
    Connection connection;
    QList<QDBusObjectPath> items;
    if (!connection.isOpen() || !connection.find(key, &items))
        return Result::Unavailable;
    if (items.isEmpty())
        return Result::NotFound;
    return connection.secret(items.first(), data) ? Result::Found : Result::Unavailable;
#else
    Q_UNUSED(key);
    Q_UNUSED(data);
    return Result::Unavailable;
#endif
}

bool write(const QString &key, const QByteArray &data, const QString &label)
{
    if (!isAvailable())
        return false;
#if defined(Q_OS_MACOS)
    CFMutableDictionaryRef query = baseQuery(key);
    CFDataRef value = data.toCFData();
    CFMutableDictionaryRef update = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                                              &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(update, kSecValueData, value);
    OSStatus status = SecItemUpdate(query, update);
    if (status == errSecItemNotFound) {
        CFStringRef labelString = label.toCFString();
        CFDictionarySetValue(query, kSecValueData, value);
        CFDictionarySetValue(query, kSecAttrLabel, labelString);
        status = SecItemAdd(query, nullptr);
        CFRelease(labelString);
    }
    CFRelease(update);
    CFRelease(value);
    CFRelease(query);
    return status == errSecSuccess;
#elif defined(SPACEPENGUIN_HAVE_DBUS)
    Connection connection;
    if (!connection.isOpen())
        return false;
    const QDBusObjectPath collection = connection.defaultCollection();
    if (collection.path().isEmpty())
        return false;
    // CreateItem with "replace" swaps the item with the same attributes.
    return connection.create(collection, key, data, label);
#else
    Q_UNUSED(key);
    Q_UNUSED(data);
    Q_UNUSED(label);
    return false;
#endif
}

bool remove(const QString &key)
{
    if (!isAvailable())
        return false;
#if defined(Q_OS_MACOS)
    CFMutableDictionaryRef query = baseQuery(key);
    const OSStatus status = SecItemDelete(query);
    CFRelease(query);
    return status == errSecSuccess || status == errSecItemNotFound;
#elif defined(SPACEPENGUIN_HAVE_DBUS)
    Connection connection;
    QList<QDBusObjectPath> items;
    if (!connection.isOpen() || !connection.find(key, &items))
        return false;
    bool ok = true;
    for (const QDBusObjectPath &item : std::as_const(items))
        ok = connection.deleteItem(item) && ok;
    return ok;
#else
    Q_UNUSED(key);
    return false;
#endif
}

}

#if defined(SPACEPENGUIN_HAVE_DBUS) && !defined(Q_OS_MACOS)
#    include "secretstore.moc"
#endif

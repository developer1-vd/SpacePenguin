#include "chromeextensions.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QWebEngineProfile>
#include <QtEndian>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#    include <QWebEngineExtensionInfo>
#    include <QWebEngineExtensionManager>
#    define SPACEPENGUIN_EXTENSIONS 1
#endif

namespace spacepenguin {
namespace {

#ifdef SPACEPENGUIN_EXTENSIONS
QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

/** "__MSG_appName__" from the extension's default locale, as Chrome shows it. */
QString localized(const QString &text, const QString &folder, const QJsonObject &manifest)
{
    static const QRegularExpression message(QStringLiteral("^__MSG_(\\w+)__$"));
    const QRegularExpressionMatch match = message.match(text);
    if (!match.hasMatch())
        return text;
    const QString locale = manifest.value(QStringLiteral("default_locale")).toString(QStringLiteral("en"));
    const QJsonObject messages =
        readJson(QDir(folder).filePath(QStringLiteral("_locales/%1/messages.json").arg(locale)));
    for (auto it = messages.begin(); it != messages.end(); ++it) {
        if (it.key().compare(match.captured(1), Qt::CaseInsensitive) == 0)
            return it.value().toObject().value(QStringLiteral("message")).toString(text);
    }
    return text;
}

/** The extension's own icon, from its manifest: the largest one up to 48 pixels. */
QIcon manifestIcon(const QString &folder, const QJsonObject &manifest)
{
    QJsonObject icons = manifest.value(QStringLiteral("icons")).toObject();
    const QJsonValue action = manifest.value(QStringLiteral("action")).toObject().value(QStringLiteral("default_icon"));
    if (icons.isEmpty() && action.isObject())
        icons = action.toObject();
    QString best;
    int bestSize = 0;
    for (auto it = icons.begin(); it != icons.end(); ++it) {
        const int size = it.key().toInt();
        if ((size <= 48 && size > bestSize) || best.isEmpty()) {
            best = it.value().toString();
            bestSize = size;
        }
    }
    if (best.isEmpty() && action.isString())
        best = action.toString();
    const QString file = QDir(folder).filePath(best);
    return !best.isEmpty() && QFile::exists(file) ? QIcon(file) : QIcon();
}

/** The zip inside a .crx (versions 2 and 3); a zip is returned as it is. */
QByteArray zipOf(const QByteArray &data)
{
    if (!data.startsWith("Cr24"))
        return data;
    if (data.size() < 16)
        return {};
    const auto u32 = [&data](int at) { return qFromLittleEndian<quint32>(data.constData() + at); };
    const quint32 version = u32(4);
    qsizetype start = 0;
    if (version == 3)
        start = 12 + qsizetype(u32(8));
    else if (version == 2)
        start = 16 + qsizetype(u32(8)) + qsizetype(u32(12));
    return start > 0 && start < data.size() ? data.mid(start) : QByteArray();
}
#endif

} // namespace

ChromeExtensions::ChromeExtensions(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent)
    , m_profile(profile)
{
#ifdef SPACEPENGUIN_EXTENSIONS
    if (QWebEngineExtensionManager *manager = m_profile->extensionManager()) {
        connect(manager, &QWebEngineExtensionManager::installFinished, this,
                [this](const QWebEngineExtensionInfo &extension) {
                    if (!extension.error().isEmpty()) {
                        emit failed(tr("The extension could not be added: %1").arg(extension.error()));
                    } else {
                        // Qt installs extensions switched off; someone who added one wants it on.
                        // Not from inside the manager's own signal, which Qt does not survive.
                        const QString id = extension.id();
                        QTimer::singleShot(0, this, [this, id] { setEnabled(id, true); });
                        emit installed(extension.name());
                    }
                    emit changed();
                });
        connect(manager, &QWebEngineExtensionManager::uninstallFinished, this,
                &ChromeExtensions::changed);
        connect(manager, &QWebEngineExtensionManager::loadFinished, this, &ChromeExtensions::changed);
        connect(manager, &QWebEngineExtensionManager::unloadFinished, this, &ChromeExtensions::changed);
    }
#endif
}

bool ChromeExtensions::isSupported()
{
#ifdef SPACEPENGUIN_EXTENSIONS
    return true;
#else
    return false;
#endif
}

QString ChromeExtensions::webStoreId(const QUrl &url)
{
    // chromewebstore.google.com/detail/<name>/<id>, and the old chrome.google.com/webstore/detail/…
    static const QRegularExpression detail(QStringLiteral("/detail/(?:[^/]+/)?([a-p]{32})(?:[/?#]|$)"));
    const QString host = url.host();
    if (host != QLatin1String("chromewebstore.google.com") && host != QLatin1String("chrome.google.com"))
        return {};
    const QRegularExpressionMatch match = detail.match(url.path());
    return match.hasMatch() ? match.captured(1) : QString();
}

QList<ChromeExtensions::Extension> ChromeExtensions::extensions() const
{
    QList<Extension> list;
#ifdef SPACEPENGUIN_EXTENSIONS
    QWebEngineExtensionManager *manager = m_profile->extensionManager();
    if (!manager)
        return list;
    for (const QWebEngineExtensionInfo &info : manager->extensions()) {
        // Chromium's own (the PDF viewer and the like) are not the person's to manage.
        if (!info.isInstalled())
            continue;
        const QJsonObject manifest = readJson(QDir(info.path()).filePath(QStringLiteral("manifest.json")));
        Extension extension;
        extension.id = info.id();
        extension.name = localized(info.name(), info.path(), manifest);
        extension.description = localized(info.description(), info.path(), manifest);
        extension.path = info.path();
        extension.error = info.error();
        extension.popupUrl = info.actionPopupUrl();
        extension.icon = manifestIcon(info.path(), manifest);
        extension.enabled = info.isEnabled();
        list.append(extension);
    }
#endif
    return list;
}

bool ChromeExtensions::isInstalled(const QString &id) const
{
    for (const Extension &extension : extensions()) {
        if (extension.id == id)
            return true;
    }
    return false;
}

void ChromeExtensions::install(const QString &path)
{
#ifdef SPACEPENGUIN_EXTENSIONS
    if (QFileInfo(path).isDir()) {
        if (QWebEngineExtensionManager *manager = m_profile->extensionManager())
            manager->installExtension(path);
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit failed(tr("Could not read %1.").arg(QDir::toNativeSeparators(path)));
        return;
    }
    installArchive(file.readAll(), QFileInfo(path).completeBaseName());
#else
    Q_UNUSED(path)
#endif
}

void ChromeExtensions::installArchive(const QByteArray &crxOrZip, const QString &name)
{
#ifdef SPACEPENGUIN_EXTENSIONS
    const QByteArray zip = zipOf(crxOrZip);
    if (!zip.startsWith("PK")) {
        emit failed(tr("“%1” is not a Chrome extension (.crx or .zip).").arg(name));
        return;
    }
    // The manager unpacks it into the profile; the zip is only needed until then.
    const QString folder = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                               .filePath(QStringLiteral("spacepenguin-extensions"));
    QDir().mkpath(folder);
    QString safeName = name;
    safeName.replace(QRegularExpression(QStringLiteral("[^\\w.-]")), QStringLiteral("_"));
    const QString zipPath = QDir(folder).filePath(safeName + QStringLiteral(".zip"));
    QFile file(zipPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(zip) != zip.size()) {
        emit failed(tr("Could not write %1.").arg(QDir::toNativeSeparators(zipPath)));
        return;
    }
    file.close();
    if (QWebEngineExtensionManager *manager = m_profile->extensionManager())
        manager->installExtension(zipPath);
#else
    Q_UNUSED(crxOrZip)
    Q_UNUSED(name)
#endif
}

void ChromeExtensions::installFromWebStore(const QString &id, const QString &name)
{
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    // Google's update service hands out the store's .crx files, as it does to Chrome.
    QUrl url(QStringLiteral("https://clients2.google.com/service/update2/crx"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("response"), QStringLiteral("redirect"));
    query.addQueryItem(QStringLiteral("prodversion"), QString::fromLatin1(qWebEngineChromiumVersion()));
    query.addQueryItem(QStringLiteral("acceptformat"), QStringLiteral("crx3"));
    query.addQueryItem(QStringLiteral("x"), QStringLiteral("id=%1&installsource=ondemand&uc").arg(id));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, name] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(tr("Could not download “%1”: %2").arg(name, reply->errorString()));
            return;
        }
        installArchive(reply->readAll(), name);
    });
}

void ChromeExtensions::setEnabled(const QString &id, bool enabled)
{
#ifdef SPACEPENGUIN_EXTENSIONS
    if (QWebEngineExtensionManager *manager = m_profile->extensionManager()) {
        for (const QWebEngineExtensionInfo &info : manager->extensions()) {
            if (info.id() == id)
                manager->setExtensionEnabled(info, enabled);
        }
    }
    emit changed();
#else
    Q_UNUSED(id)
    Q_UNUSED(enabled)
#endif
}

void ChromeExtensions::uninstall(const QString &id)
{
#ifdef SPACEPENGUIN_EXTENSIONS
    if (QWebEngineExtensionManager *manager = m_profile->extensionManager()) {
        for (const QWebEngineExtensionInfo &info : manager->extensions()) {
            if (info.id() == id)
                manager->uninstallExtension(info);
        }
    }
#else
    Q_UNUSED(id)
#endif
}

}

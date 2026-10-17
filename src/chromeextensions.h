#pragma once

#include <QIcon>
#include <QList>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QWebEngineProfile;

namespace spacepenguin {

/**
 * Chrome extensions (Manifest V3), through QtWebEngine's extension manager, which
 * Qt 6.10 introduced. Built against an older Qt, isSupported() is false and the
 * rest does nothing, so the browser still builds with a distribution's Qt.
 *
 * Experimental, and off unless SPACEPENGUIN_CHROME_EXTENSIONS=1: in Qt 6.10.3 and
 * 6.11.3, turning any extension on (QWebEngineExtensionManager::setExtensionEnabled)
 * crashes QtWebEngine, on a desktop as in a five-line program. Qt also keeps
 * installed extensions in the profile's "Extensions" folder, which on Windows and
 * macOS is the same folder as the user scripts' "extensions"; that needs settling
 * before this is turned on for everyone.
 *
 * Extensions can come from a folder, a .zip, a .crx file, or the Chrome Web Store:
 * a store page offers "Add to SpacePenguin", which downloads the extension's .crx
 * from Google's update service, as other Chromium browsers do. A .crx is a zip
 * behind a signed header; the header is dropped and the zip installed.
 *
 * QtWebEngine does not load extensions into private (off-the-record) profiles.
 */
class ChromeExtensions : public QObject
{
    Q_OBJECT

public:
    struct Extension {
        QString id;
        QString name;
        QString description;
        QString path;
        QString error;
        QUrl popupUrl; // the page its toolbar button opens, if any
        QIcon icon;
        bool enabled = false;
    };

    explicit ChromeExtensions(QWebEngineProfile *profile, QObject *parent = nullptr);

    /** Built with Qt 6.10+ and turned on with SPACEPENGUIN_CHROME_EXTENSIONS=1. */
    static bool isSupported();
    /** Built with a Qt that has extensions, whether or not they are turned on. */
    static bool isAvailable();
    /** The store's ID for an extension page of the Chrome Web Store; empty otherwise. */
    static QString webStoreId(const QUrl &url);

    QList<Extension> extensions() const;
    bool isInstalled(const QString &id) const;

    /** A folder with a manifest.json, a .zip of one, or a .crx. */
    void install(const QString &path);
    void installFromWebStore(const QString &id, const QString &name);
    void setEnabled(const QString &id, bool enabled);
    void uninstall(const QString &id);

signals:
    void changed();
    /** Something went wrong; @p message says what, for people. */
    void failed(const QString &message);
    void installed(const QString &name);

private:
    void installArchive(const QByteArray &crxOrZip, const QString &name);

    QWebEngineProfile *m_profile;
    QNetworkAccessManager *m_network = nullptr;
};

}

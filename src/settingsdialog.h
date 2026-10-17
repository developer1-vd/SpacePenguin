#pragma once

#include <QDialog>

class QLabel;
class QListWidget;
class QStackedWidget;
class QWebEngineProfile;
class QWidget;

namespace spacepenguin {

class AdBlocker;
class BookmarkManager;
class CookieManager;
class DataSaverManager;
class DownloadManager;
class HistoryManager;
class UserScripts;

/**
 * Preferences window. Every control takes effect immediately; there is nothing to
 * apply or cancel, and no control is shown that the browser does not honour.
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(
        AdBlocker *adBlocker,
        BookmarkManager *bookmarkManager,
        CookieManager *cookieManager,
        DataSaverManager *dataSaverManager,
        DownloadManager *downloadManager,
        HistoryManager *historyManager,
        UserScripts *userScripts,
        QWidget *parent = nullptr);
    ~SettingsDialog() override;

    /** Enables "Clear cache"; the cache belongs to the profile, not to a manager. */
    void setProfile(QWebEngineProfile *profile);

signals:
    void extensionsRequested();

private:
    void setupTabs();
    void addPage(const QIcon &icon, const QString &title, QWidget *page);
    QWidget *createGeneralTab();
    QWidget *createAppearanceTab();
    QWidget *createPrivacyTab();
    QWidget *createExtensionsTab();
    QWidget *createDownloadsTab();
    QWidget *createAboutTab();
    void updateBlockingSummary();

    AdBlocker *m_adBlocker = nullptr;
    BookmarkManager *m_bookmarkManager = nullptr;
    CookieManager *m_cookieManager = nullptr;
    DataSaverManager *m_dataSaverManager = nullptr;
    DownloadManager *m_downloadManager = nullptr;
    HistoryManager *m_historyManager = nullptr;
    UserScripts *m_userScripts = nullptr;
    QWebEngineProfile *m_profile = nullptr;

    QListWidget *m_nav = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_blockingSummary = nullptr;
    QWidget *m_clearCacheButton = nullptr;
};

}

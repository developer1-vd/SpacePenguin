#pragma once

#include <QDialog>

class QTabWidget;
class QWidget;

namespace spacepenguin {

class AdBlocker;
class BookmarkManager;
class CookieManager;
class DataSaverManager;
class DownloadManager;
class HistoryManager;
class UserScripts;

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

private:
    void setupTabs();
    QWidget *createGeneralTab();
    QWidget *createAppearanceTab();
    QWidget *createPrivacyTab();
    QWidget *createExtensionsTab();
    QWidget *createDownloadsTab();
    QWidget *createAboutTab();

    QTabWidget *m_tabWidget = nullptr;
};

}
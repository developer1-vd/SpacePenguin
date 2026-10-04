#include "settingsdialog.h"

#include "adblocker.h"
#include "bookmarkmanager.h"
#include "cookiemanager.h"
#include "datasavermanager.h"
#include "downloadmanager.h"
#include "historymanager.h"
#include "userextensions.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace spacepenguin {

SettingsDialog::SettingsDialog(
    AdBlocker *adBlocker,
    BookmarkManager *bookmarkManager,
    CookieManager *cookieManager,
    DataSaverManager *dataSaverManager,
    DownloadManager *downloadManager,
    HistoryManager *historyManager,
    UserScripts *userScripts,
    QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(640, 480);

    setupTabs();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupTabs()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createGeneralTab(), tr("General"));
    m_tabWidget->addTab(createAppearanceTab(), tr("Appearance"));
    m_tabWidget->addTab(createPrivacyTab(), tr("Privacy"));
    m_tabWidget->addTab(createExtensionsTab(), tr("Extensions"));
    m_tabWidget->addTab(createDownloadsTab(), tr("Downloads"));
    m_tabWidget->addTab(createAboutTab(), tr("About"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_tabWidget);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttonBox);
}

QWidget *SettingsDialog::createGeneralTab()
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);

    auto *group = new QGroupBox(tr("Startup"));
    auto *form = new QFormLayout(group);

    auto *homeUrlEdit = new QLineEdit;
    homeUrlEdit->setPlaceholderText("sp://start/");
    form->addRow(tr("Home page:"), homeUrlEdit);

    auto *restoreTabsCheck = new QCheckBox(tr("Restore previous session tabs on startup"));
    restoreTabsCheck->setChecked(true);
    form->addRow(restoreTabsCheck);

    layout->addWidget(group);

    group = new QGroupBox(tr("Search"));
    form = new QFormLayout(group);

    auto *searchEngineCombo = new QComboBox;
    searchEngineCombo->addItem("DuckDuckGo", "https://duckduckgo.com/?q={query}");
    searchEngineCombo->addItem("Google", "https://www.google.com/search?q={query}");
    searchEngineCombo->addItem("Bing", "https://www.bing.com/search?q={query}");
    searchEngineCombo->addItem("Startpage", "https://www.startpage.com/sp/search?q={query}");
    form->addRow(tr("Default search engine:"), searchEngineCombo);

    layout->addWidget(group);
    layout->addStretch();

    return widget;
}

QWidget *SettingsDialog::createAppearanceTab()
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);

    auto *group = new QGroupBox(tr("Theme"));
    auto *form = new QFormLayout(group);

    auto *themeCombo = new QComboBox;
    themeCombo->addItem(tr("Follow system"), "system");
    themeCombo->addItem(tr("Light"), "light");
    themeCombo->addItem(tr("Dark"), "dark");
    form->addRow(tr("Theme:"), themeCombo);

    layout->addWidget(group);

    group = new QGroupBox(tr("Toolbar"));
    auto *vlayout = new QVBoxLayout(group);

    auto *showHomeBtn = new QCheckBox(tr("Show home button"));
    showHomeBtn->setChecked(true);
    vlayout->addWidget(showHomeBtn);

    auto *showDownloadsBtn = new QCheckBox(tr("Show downloads button"));
    showDownloadsBtn->setChecked(true);
    vlayout->addWidget(showDownloadsBtn);

    layout->addWidget(group);
    layout->addStretch();

    return widget;
}

QWidget *SettingsDialog::createPrivacyTab()
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);

    auto *group = new QGroupBox(tr("Blocking"));
    auto *form = new QFormLayout(group);

    auto *blockEnabled = new QCheckBox(tr("Enable ad blocking"));
    blockEnabled->setChecked(true);
    form->addRow(blockEnabled);

    auto *cosmeticEnabled = new QCheckBox(tr("Enable cosmetic filtering (element hiding)"));
    cosmeticEnabled->setChecked(true);
    form->addRow(cosmeticEnabled);

    auto *filterListEdit = new QLineEdit;
    filterListEdit->setPlaceholderText(tr("Path to filter list (optional)"));
    form->addRow(tr("Custom filter list:"), filterListEdit);

    layout->addWidget(group);

    group = new QGroupBox(tr("Cookies"));
    auto *vlayout = new QVBoxLayout(group);

    auto *clearAllCookiesBtn = new QPushButton(tr("Clear all cookies"));
    clearAllCookiesBtn->setToolTip(tr("Delete all cookies"));
    vlayout->addWidget(clearAllCookiesBtn);

    auto *clearSessionCookiesBtn = new QPushButton(tr("Clear session cookies"));
    clearSessionCookiesBtn->setToolTip(tr("Delete cookies that expire when the browser closes"));
    vlayout->addWidget(clearSessionCookiesBtn);

    layout->addWidget(group);

    group = new QGroupBox(tr("History"));
    vlayout = new QVBoxLayout(group);

    auto *clearHistoryBtn = new QPushButton(tr("Clear browsing history"));
    clearHistoryBtn->setToolTip(tr("Delete all history entries"));
    vlayout->addWidget(clearHistoryBtn);

    auto *clearSessionHistoryBtn = new QPushButton(tr("Clear session history"));
    clearSessionHistoryBtn->setToolTip(tr("Delete history from this session"));
    vlayout->addWidget(clearSessionHistoryBtn);

    layout->addWidget(group);

    group = new QGroupBox(tr("Data Saver"));
    form = new QFormLayout(group);

    auto *dataSaverCombo = new QComboBox;
    dataSaverCombo->addItem(tr("Off"), "off");
    dataSaverCombo->addItem(tr("Standard"), "standard");
    dataSaverCombo->addItem(tr("Aggressive"), "aggressive");
    form->addRow(tr("Mode:"), dataSaverCombo);

    layout->addWidget(group);
    layout->addStretch();

    return widget;
}

QWidget *SettingsDialog::createExtensionsTab()
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);

    auto *group = new QGroupBox(tr("User Scripts"));
    auto *vlayout = new QVBoxLayout(group);

    auto *list = new QListWidget;
    vlayout->addWidget(list);

    auto *btnLayout = new QHBoxLayout;
    auto *addBtn = new QPushButton(tr("Add script..."));
    auto *removeBtn = new QPushButton(tr("Remove"));
    auto *enableBtn = new QPushButton(tr("Enable/Disable"));
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addWidget(enableBtn);
    btnLayout->addStretch();
    vlayout->addLayout(btnLayout);

    layout->addWidget(group);
    layout->addStretch();

    return widget;
}

QWidget *SettingsDialog::createDownloadsTab()
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);

    auto *group = new QGroupBox(tr("Download Location"));
    auto *form = new QFormLayout(group);

    auto *locationEdit = new QLineEdit;
    form->addRow(tr("Folder:"), locationEdit);

    auto *browseBtn = new QPushButton(tr("Browse..."));
    form->addRow(browseBtn);

    auto *askLocationCheck = new QCheckBox(tr("Ask where to save each file"));
    form->addRow(askLocationCheck);

    layout->addWidget(group);

    group = new QGroupBox(tr("Behavior"));
    auto *vlayout = new QVBoxLayout(group);

    auto *openAfterCheck = new QCheckBox(tr("Open files after download"));
    vlayout->addWidget(openAfterCheck);

    auto *clearFinishedCheck = new QCheckBox(tr("Auto-remove finished downloads"));
    vlayout->addWidget(clearFinishedCheck);

    layout->addWidget(group);
    layout->addStretch();

    return widget;
}

QWidget *SettingsDialog::createAboutTab()
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);

    auto *label = new QLabel;
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setText(
        "<h2>SpacePenguin</h2>"
        "<p>Version 0.1.0</p>"
        "<p>A secure, lightweight, and usable browser written in Qt.</p>"
        "<p>Built with Qt 6 and QtWebEngine (Chromium).</p>"
        "<p>Licensed under the GNU General Public License v3.</p>"
        "<p><a href=\"https://github.com/\">Source code</a> | "
        "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">License</a></p>"
    );
    label->setOpenExternalLinks(true);
    layout->addWidget(label);
    layout->addStretch();

    return widget;
}

}
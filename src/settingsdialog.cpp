#include "settingsdialog.h"

#include "adblocker.h"
#include "classroom.h"
#include "filterlists.h"
#include "updatechecker.h"
#include "bookmarkmanager.h"
#include "cookiemanager.h"
#include "datasavermanager.h"
#include "downloadmanager.h"
#include "historymanager.h"
#include "icons.h"
#include "preferences.h"
#include "roles.h"
#include "theme.h"
#include "tor.h"
#include "userextensions.h"
#include "webapps.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QPixmap>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineProfile>

#include <functional>

namespace spacepenguin {
namespace {

const QString kRepositoryUrl = QStringLiteral("https://github.com/developer1-vd/SpacePenguin");

QLabel *sectionTitle(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

QLabel *mutedLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("muted", true);
    label->setWordWrap(true);
    return label;
}

/** A scrollable page with a large title and an optional one-line description. */
QWidget *pageShell(const QString &title, const QString &subtitle, QVBoxLayout **contentLayout)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(10);

    auto *heading = new QLabel(title);
    heading->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(heading);
    if (!subtitle.isEmpty()) {
        auto *description = new QLabel(subtitle);
        description->setObjectName(QStringLiteral("pageSubtitle"));
        description->setWordWrap(true);
        layout->addWidget(description);
    }
    layout->addSpacing(6);

    scroll->setWidget(page);
    *contentLayout = layout;
    return scroll;
}

bool confirm(QWidget *parent, const QString &title, const QString &text)
{
    return QMessageBox::question(parent, title, text,
                                 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        == QMessageBox::Yes;
}

} // namespace

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
    , m_adBlocker(adBlocker)
    , m_bookmarkManager(bookmarkManager)
    , m_cookieManager(cookieManager)
    , m_dataSaverManager(dataSaverManager)
    , m_downloadManager(downloadManager)
    , m_historyManager(historyManager)
    , m_userScripts(userScripts)
{
    setWindowTitle(tr("Settings"));
    resize(820, 600);
    setMinimumSize(680, 480);

    setupTabs();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setProfile(QWebEngineProfile *profile)
{
    m_profile = profile;
    if (m_clearCacheButton)
        m_clearCacheButton->setEnabled(m_profile != nullptr);
}

void SettingsDialog::setupTabs()
{
    m_nav = new QListWidget;
    m_nav->setObjectName(QStringLiteral("settingsNav"));
    m_nav->setFixedWidth(200);
    m_nav->setIconSize(QSize(18, 18));
    m_nav->setFrameShape(QFrame::NoFrame);

    m_pages = new QStackedWidget;

    addPage(Icons::icon(Icons::Home), tr("General"), createGeneralTab());
    addPage(Icons::icon(Icons::Penguin), tr("Appearance"), createAppearanceTab());
    addPage(Icons::icon(Icons::Shield), tr("Privacy & Security"), createPrivacyTab());
    addPage(Icons::icon(Icons::Download), tr("Downloads"), createDownloadsTab());
    addPage(Icons::icon(Icons::Star), tr("Made for you"), createRolesTab());
    addPage(Icons::icon(Icons::Globe), tr("Apps"), createAppsTab());
    addPage(Icons::icon(Icons::Extensions), tr("Extensions"), createExtensionsTab());
    addPage(Icons::icon(Icons::Info), tr("About"), createAboutTab());

    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    auto *navPanel = new QWidget;
    auto *navLayout = new QVBoxLayout(navPanel);
    navLayout->setContentsMargins(10, 14, 6, 10);
    navLayout->addWidget(m_nav);
    body->addWidget(navPanel);
    body->addWidget(m_pages, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->addLayout(body, 1);
    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(16, 0, 16, 0);
    buttons->addWidget(mutedLabel(tr("Changes are saved as you make them.")));
    buttons->addWidget(buttonBox);
    layout->addLayout(buttons);

    // Enter in a text field must only commit that field, never press another button.
    for (QPushButton *button : findChildren<QPushButton *>())
        button->setAutoDefault(false);
}

void SettingsDialog::addPage(const QIcon &icon, const QString &title, QWidget *page)
{
    new QListWidgetItem(icon, title, m_nav);
    m_pages->addWidget(page);
}

QWidget *SettingsDialog::createGeneralTab()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = pageShell(tr("General"), tr("Start-up, home page and search."), &layout);
    Preferences *preferences = Preferences::instance();

    layout->addWidget(sectionTitle(tr("On start-up")));
    auto *restore = new QRadioButton(tr("Continue where you left off"));
    auto *openHome = new QRadioButton(tr("Open the home page"));
    restore->setChecked(preferences->restoreSession());
    openHome->setChecked(!preferences->restoreSession());
    connect(restore, &QRadioButton::toggled, this,
            [](bool checked) { Preferences::instance()->setRestoreSession(checked); });
    layout->addWidget(restore);
    layout->addWidget(openHome);

    layout->addWidget(sectionTitle(tr("Updates")));
    auto *updates = new QCheckBox(tr("Tell me when a new version is out"));
    updates->setChecked(preferences->checkForUpdates());
    connect(updates, &QCheckBox::toggled, this, [](bool on) {
        Preferences::instance()->setCheckForUpdates(on);
        if (on)
            UpdateChecker::instance()->checkIfDue();
    });
    layout->addWidget(updates);
    layout->addWidget(mutedLabel(tr("Once a day, SpacePenguin asks GitHub for its newest release. It never "
                                    "installs anything itself; on Linux, run install.sh again to update.")));

    layout->addWidget(sectionTitle(tr("Home page")));
    auto *homeRow = new QHBoxLayout;
    auto *homeEdit = new QLineEdit(preferences->homePage());
    homeEdit->setPlaceholderText(QStringLiteral("sp://start/"));
    homeEdit->setClearButtonEnabled(true);
    connect(homeEdit, &QLineEdit::editingFinished, this, [homeEdit] {
        Preferences::instance()->setHomePage(homeEdit->text());
        homeEdit->setText(Preferences::instance()->homePage());
    });
    auto *defaultHome = new QPushButton(tr("Use start page"));
    connect(defaultHome, &QPushButton::clicked, this, [homeEdit] {
        Preferences::instance()->setHomePage(QString());
        homeEdit->setText(Preferences::instance()->homePage());
    });
    homeRow->addWidget(homeEdit, 1);
    homeRow->addWidget(defaultHome);
    layout->addLayout(homeRow);
    layout->addWidget(mutedLabel(tr("Opened by the Home button and in every new tab.")));

    layout->addWidget(sectionTitle(tr("Search engine")));
    auto *engineRow = new QHBoxLayout;
    auto *engineCombo = new QComboBox;
    auto *addEngine = new QPushButton(Icons::icon(Icons::Plus), tr("Add…"));
    auto *removeEngine = new QPushButton(Icons::icon(Icons::Trash), tr("Remove"));
    engineRow->addWidget(engineCombo, 1);
    engineRow->addWidget(addEngine);
    engineRow->addWidget(removeEngine);
    layout->addLayout(engineRow);

    // The list: built-in engines, then added ones (data: template, and whether added).
    const auto fillEngines = [engineCombo, removeEngine] {
        const QSignalBlocker blocker(engineCombo);
        engineCombo->clear();
        const QString current = Preferences::instance()->searchTemplate();
        for (const SearchEngine &engine : Preferences::builtInSearchEngines())
            engineCombo->addItem(engine.name, engine.searchTemplate);
        const QList<SearchEngine> added = Preferences::addedSearchEngines();
        if (!added.isEmpty())
            engineCombo->insertSeparator(engineCombo->count());
        for (const SearchEngine &engine : added)
            engineCombo->addItem(engine.name, engine.searchTemplate);
        int index = engineCombo->findData(current);
        if (index < 0) {
            // An address typed in an earlier version: shown under its site's name.
            engineCombo->addItem(QUrl(current).host(), current);
            index = engineCombo->count() - 1;
        }
        engineCombo->setCurrentIndex(index);
        bool isAdded = false;
        for (const SearchEngine &engine : added)
            isAdded = isAdded || engine.searchTemplate == current;
        removeEngine->setEnabled(isAdded);
    };
    fillEngines();
    connect(engineCombo, &QComboBox::currentIndexChanged, this, [engineCombo, fillEngines](int index) {
        Preferences::instance()->setSearchTemplate(engineCombo->itemData(index).toString());
        // Refilled once the combo box is done announcing the change.
        QTimer::singleShot(0, engineCombo, fillEngines);
    });
    connect(addEngine, &QPushButton::clicked, this, [this, fillEngines] {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Add Search Engine"));
        auto *form = new QFormLayout(&dialog);
        auto *name = new QLineEdit;
        name->setPlaceholderText(tr("Example Search"));
        auto *address = new QLineEdit;
        address->setPlaceholderText(QStringLiteral("https://example.com/search?q=%s"));
        address->setMinimumWidth(360);
        form->addRow(tr("Name"), name);
        form->addRow(tr("Address"), address);
        form->addRow(QString(), mutedLabel(tr("Search the site for anything, then copy the address of the "
                                              "results and put %s (or {query}) where your words were.")));
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
        buttons->addButton(tr("Add"), QDialogButtonBox::AcceptRole);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, name, address] {
            const QString searchTemplate = Preferences::normalizeSearchTemplate(address->text());
            if (searchTemplate.isEmpty()) {
                QMessageBox::warning(&dialog, tr("Add Search Engine"),
                                     tr("The address must start with https:// and contain %s or {query}."));
                return;
            }
            const QString title = name->text().trimmed().isEmpty() ? QUrl(searchTemplate).host()
                                                                    : name->text().trimmed();
            Preferences::instance()->addSearchEngine(title, searchTemplate);
            Preferences::instance()->setSearchTemplate(searchTemplate);
            dialog.accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);
        dialog.exec();
        fillEngines();
    });
    connect(removeEngine, &QPushButton::clicked, this, [fillEngines] {
        Preferences::instance()->removeSearchEngine(Preferences::instance()->searchTemplate());
        fillEngines();
    });

    layout->addWidget(mutedLabel(tr("Anything typed in the address bar that is not an "
                                    "address is sent here. Switch quickly from the menu (Search With).")));
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createAppearanceTab()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = pageShell(tr("Appearance"), tr("How the browser window looks."), &layout);
    Preferences *preferences = Preferences::instance();

    layout->addWidget(sectionTitle(tr("Theme")));
    auto *group = new QButtonGroup(page);
    const Theme::Mode stored = Theme::storedMode();
    for (const Theme::Mode mode : Theme::modes()) {
        auto *button = new QRadioButton(Theme::modeLabel(mode));
        button->setChecked(mode == stored);
        group->addButton(button);
        connect(button, &QRadioButton::toggled, this, [mode](bool checked) {
            if (!checked)
                return;
            Theme::storeMode(mode);
            Theme::apply(mode);
            Preferences::instance()->notifyChanged();
        });
        layout->addWidget(button);
    }
    layout->addWidget(mutedLabel(tr("The theme colours the browser itself. Websites that "
                                    "support dark mode follow your system setting.")));

    layout->addWidget(sectionTitle(tr("Colour")));
    auto *colourRow = new QHBoxLayout;
    auto *colour = new QComboBox;
    for (const Theme::Palette &palette : Theme::palettes()) {
        QPixmap swatch(14, 14);
        swatch.fill(palette.lightAccent);
        colour->addItem(QIcon(swatch), palette.name, palette.id);
        if (palette.id == Theme::storedPalette())
            colour->setCurrentIndex(colour->count() - 1);
    }
    connect(colour, &QComboBox::currentIndexChanged, this, [colour] {
        Theme::storePalette(colour->currentData().toString());
        Theme::apply(Theme::storedMode());
        Preferences::instance()->notifyChanged();
    });
    colourRow->addWidget(colour);
    colourRow->addStretch(1);
    layout->addLayout(colourRow);
    layout->addWidget(mutedLabel(tr("More in the SpacePenguin Store, under the puzzle piece in "
                                    "the toolbar.")));

    layout->addWidget(sectionTitle(tr("Tabs")));
    auto *sideTabs = new QCheckBox(tr("Show tabs on the side"));
    sideTabs->setChecked(preferences->sideTabs());
    connect(sideTabs, &QCheckBox::toggled, this,
            [](bool checked) { Preferences::instance()->setSideTabs(checked); });
    layout->addWidget(sideTabs);
    layout->addWidget(mutedLabel(tr("A panel down the left of the window, with room for long "
                                    "titles and many tabs. Collapse it to just the icons with "
                                    "its arrow button.")));

    layout->addWidget(sectionTitle(tr("Toolbar")));
    auto *homeButton = new QCheckBox(tr("Show the Home button"));
    homeButton->setChecked(preferences->showHomeButton());
    connect(homeButton, &QCheckBox::toggled, this,
            [](bool checked) { Preferences::instance()->setShowHomeButton(checked); });
    layout->addWidget(homeButton);

    auto *bookmarksBar = new QCheckBox(tr("Show the bookmarks bar"));
    bookmarksBar->setChecked(preferences->showBookmarksBar());
    connect(bookmarksBar, &QCheckBox::toggled, this,
            [](bool checked) { Preferences::instance()->setShowBookmarksBar(checked); });
    layout->addWidget(bookmarksBar);

    auto *menuBar = new QCheckBox(tr("Show the menu bar"));
    menuBar->setChecked(preferences->showMenuBar());
    connect(menuBar, &QCheckBox::toggled, this,
            [](bool checked) { Preferences::instance()->setShowMenuBar(checked); });
    layout->addWidget(menuBar);
    auto *menuBarHint = mutedLabel(tr("Everything in the menu bar is also in the ☰ menu."));
    layout->addWidget(menuBarHint);
#ifndef Q_OS_MACOS
    auto *systemTitleBar = new QCheckBox(tr("Use the system title bar and borders"));
    systemTitleBar->setChecked(preferences->useSystemTitleBar());
    connect(systemTitleBar, &QCheckBox::toggled, this,
            [](bool checked) { Preferences::instance()->setUseSystemTitleBar(checked); });
    layout->addWidget(systemTitleBar);
    layout->addWidget(mutedLabel(tr("Off: the tabs share one row with the minimize, maximize "
                                    "and close buttons, and dragging the empty part of that "
                                    "row moves the window.")));
#endif
#ifdef Q_OS_MACOS
    // The macOS menu bar is the system's and is always shown.
    menuBar->hide();
    menuBarHint->hide();
#endif

    // Keep the controls honest when the same setting changes elsewhere.
    connect(Preferences::instance(), &Preferences::changed, page,
            [sideTabs, homeButton, bookmarksBar, menuBar] {
                Preferences *current = Preferences::instance();
                sideTabs->setChecked(current->sideTabs());
                homeButton->setChecked(current->showHomeButton());
                bookmarksBar->setChecked(current->showBookmarksBar());
                menuBar->setChecked(current->showMenuBar());
            });

    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createPrivacyTab()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = pageShell(tr("Privacy & Security"),
                              tr("Content blocking, data use and your browsing data."), &layout);

    layout->addWidget(sectionTitle(tr("Logins and permissions")));
    auto *offerLogins = new QCheckBox(tr("Offer to save logins"));
    offerLogins->setChecked(Preferences::instance()->offerToSaveLogins());
    connect(offerLogins, &QCheckBox::toggled, this,
            [](bool checked) { Preferences::instance()->setOfferToSaveLogins(checked); });
    layout->addWidget(offerLogins);
    auto *loginButtons = new QHBoxLayout;
    auto *savedLogins = new QPushButton(Icons::icon(Icons::Key), tr("Saved logins…"));
    connect(savedLogins, &QPushButton::clicked, this, &SettingsDialog::savedLoginsRequested);
    auto *permissions = new QPushButton(Icons::icon(Icons::Shield), tr("Site permissions…"));
    connect(permissions, &QPushButton::clicked, this, &SettingsDialog::sitePermissionsRequested);
    loginButtons->addWidget(savedLogins);
    loginButtons->addWidget(permissions);
    loginButtons->addStretch(1);
    layout->addLayout(loginButtons);
    layout->addWidget(mutedLabel(tr("Sites ask before using your camera, microphone, location "
                                    "or notifications; your answers are kept per site.")));

    layout->addWidget(sectionTitle(tr("Ad blocking")));
    auto *blocking = new QCheckBox(tr("Block ads and trackers"));
    blocking->setChecked(Preferences::instance()->blockingEnabled());
    connect(blocking, &QCheckBox::toggled, this, [this](bool checked) {
        Preferences::instance()->setBlockingEnabled(checked);
        updateBlockingSummary();
    });
    layout->addWidget(blocking);

    auto *sponsors = new QCheckBox(tr("Skip sponsor segments in YouTube videos"));
    sponsors->setChecked(Preferences::instance()->skipSponsors());
    connect(sponsors, &QCheckBox::toggled, this,
            [](bool checked) { Preferences::instance()->setSkipSponsors(checked); });
    layout->addWidget(sponsors);
    layout->addWidget(mutedLabel(tr("Uses SponsorBlock's community timestamps. Only a short "
                                    "fragment of a hash of the video's ID is sent, never the ID "
                                    "itself, so SponsorBlock cannot tell which video you watch.")));

    auto *strip = new QCheckBox(tr("Remove tracking from links (utm_source, fbclid and the like)"));
    strip->setChecked(Preferences::instance()->stripTracking());
    connect(strip, &QCheckBox::toggled, this, [](bool on) { Preferences::instance()->setStripTracking(on); });
    layout->addWidget(strip);

    m_blockingSummary = mutedLabel(QString());
    m_blockingSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_blockingSummary);

    auto *filterButtons = new QHBoxLayout;
    auto *reload = new QPushButton(Icons::icon(Icons::Reload), tr("Reload filter list"));
    reload->setEnabled(m_adBlocker != nullptr);
    connect(reload, &QPushButton::clicked, this, [] { FilterLists::instance()->reload(); });
    auto *openFolder = new QPushButton(Icons::icon(Icons::Folder), tr("Show your filter list"));
    openFolder->setEnabled(m_adBlocker != nullptr);
    connect(openFolder, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo(m_adBlocker->listPath()).absolutePath()));
    });
    filterButtons->addWidget(reload);
    filterButtons->addWidget(openFolder);
    filterButtons->addStretch(1);
    layout->addLayout(filterButtons);

    // The well-known lists, a tick away; they are downloaded and kept up to date.
    layout->addWidget(sectionTitle(tr("Filter lists")));
    FilterLists *lists = FilterLists::instance();
    auto *listStatus = mutedLabel(QString());
    const auto describe = [lists](const FilterLists::List &list) {
        if (!lists->isSubscribed(list.id))
            return list.description;
        const QDateTime updated = lists->updated(list.id);
        if (!updated.isValid())
            return tr("%1 Downloading…").arg(list.description);
        const qint64 days = updated.daysTo(QDateTime::currentDateTime());
        return tr("%1 Updated %2.").arg(list.description,
                                        days == 0 ? tr("today") : tr("%n day(s) ago", nullptr, int(days)));
    };
    for (const FilterLists::List &list : FilterLists::catalog()) {
        auto *box = new QCheckBox(list.name);
        box->setChecked(lists->isSubscribed(list.id));
        auto *about = mutedLabel(describe(list));
        about->setContentsMargins(24, 0, 0, 4);
        connect(box, &QCheckBox::toggled, this, [lists, id = list.id](bool on) { lists->setSubscribed(id, on); });
        connect(lists, &FilterLists::changed, about, [about, describe, list] { about->setText(describe(list)); });
        layout->addWidget(box);
        layout->addWidget(about);
    }
    auto *updateNow = new QPushButton(Icons::icon(Icons::Reload), tr("Update lists now"));
    connect(updateNow, &QPushButton::clicked, this, [lists] { lists->update(true); });
    connect(lists, &FilterLists::changed, this, [this, updateNow, lists, listStatus] {
        updateNow->setEnabled(!lists->isUpdating());
        listStatus->setText(lists->isUpdating() ? tr("Downloading…") : QString());
        updateBlockingSummary();
    });
    connect(lists, &FilterLists::updateFailed, listStatus, &QLabel::setText);
    auto *updateRow = new QHBoxLayout;
    updateRow->addWidget(updateNow);
    updateRow->addWidget(listStatus, 1);
    layout->addLayout(updateRow);

    layout->addWidget(sectionTitle(tr("Sites where blocking is paused")));
    auto *paused = new QListWidget;
    paused->setMaximumHeight(110);
    auto *resume = new QPushButton(tr("Resume blocking on selected site"));
    resume->setEnabled(false);
    const auto reloadPaused = [this, paused] {
        paused->clear();
        if (m_adBlocker)
            paused->addItems(m_adBlocker->pausedHosts());
        if (paused->count() == 0) {
            auto *empty = new QListWidgetItem(tr("None — blocking runs everywhere."), paused);
            empty->setFlags(Qt::NoItemFlags);
        }
    };
    reloadPaused();
    connect(paused, &QListWidget::itemSelectionChanged, this,
            [paused, resume] { resume->setEnabled(!paused->selectedItems().isEmpty()); });
    connect(resume, &QPushButton::clicked, this, [this, paused, reloadPaused] {
        const auto items = paused->selectedItems();
        if (items.isEmpty() || !m_adBlocker)
            return;
        QStringList hosts = m_adBlocker->pausedHosts();
        hosts.removeAll(items.first()->text());
        m_adBlocker->setPausedHosts(hosts);
        reloadPaused();
    });
    layout->addWidget(paused);
    auto *pause = new QPushButton(tr("Pause on a site…"));
    pause->setEnabled(m_adBlocker != nullptr);
    connect(pause, &QPushButton::clicked, this, [this, reloadPaused] {
        const QString text = QInputDialog::getText(this, tr("Pause blocking"),
                                                   tr("Site where nothing should be blocked (for example example.com):"));
        QString host = QUrl::fromUserInput(text.trimmed()).host().toLower();
        if (host.startsWith(QLatin1String("www.")))
            host = host.mid(4);
        if (host.isEmpty())
            return;
        QStringList hosts = m_adBlocker->pausedHosts();
        if (!hosts.contains(host)) {
            hosts.append(host);
            m_adBlocker->setPausedHosts(hosts);
        }
        reloadPaused();
    });
    auto *resumeRow = new QHBoxLayout;
    resumeRow->addWidget(pause);
    resumeRow->addWidget(resume);
    resumeRow->addStretch(1);
    layout->addLayout(resumeRow);

    layout->addWidget(sectionTitle(tr("Data saver")));
    auto *dataSaver = new QComboBox;
    dataSaver->addItem(tr("Off"), static_cast<int>(DataSaverManager::Mode::Off));
    dataSaver->addItem(tr("Standard — skip site icons"),
                       static_cast<int>(DataSaverManager::Mode::Standard));
    dataSaver->addItem(tr("Aggressive — also skip images"),
                       static_cast<int>(DataSaverManager::Mode::Aggressive));
    dataSaver->setEnabled(m_dataSaverManager != nullptr);
    if (m_dataSaverManager)
        dataSaver->setCurrentIndex(dataSaver->findData(static_cast<int>(m_dataSaverManager->mode())));
    connect(dataSaver, &QComboBox::currentIndexChanged, this, [this, dataSaver] {
        if (m_dataSaverManager)
            m_dataSaverManager->setMode(
                static_cast<DataSaverManager::Mode>(dataSaver->currentData().toInt()));
    });
    layout->addWidget(dataSaver);
    layout->addWidget(mutedLabel(tr("Applies to open tabs straight away; reload a page to "
                                    "see the difference.")));

    layout->addWidget(sectionTitle(tr("Browsing data")));
    auto *dataButtons = new QHBoxLayout;
    auto *clearHistory = new QPushButton(Icons::icon(Icons::History), tr("Clear history…"));
    clearHistory->setEnabled(m_historyManager != nullptr);
    connect(clearHistory, &QPushButton::clicked, this, [this] {
        if (confirm(this, tr("Clear history"),
                    tr("Delete every page in your browsing history? This cannot be undone.")))
            m_historyManager->clearHistory();
    });
    auto *clearCookies = new QPushButton(tr("Clear cookies…"));
    clearCookies->setEnabled(m_cookieManager != nullptr);
    connect(clearCookies, &QPushButton::clicked, this, [this] {
        if (confirm(this, tr("Clear cookies"),
                    tr("Delete all cookies? You will be signed out of most websites.")))
            m_cookieManager->deleteAllCookies();
    });
    auto *manageCookies = new QPushButton(tr("Cookies by site…"));
    manageCookies->setEnabled(m_cookieManager != nullptr);
    connect(manageCookies, &QPushButton::clicked, this, [this] {
        CookiesDialog dialog(m_cookieManager, this);
        dialog.exec();
    });
    auto *clearCache = new QPushButton(tr("Clear cache"));
    clearCache->setEnabled(false);
    connect(clearCache, &QPushButton::clicked, this, [this, clearCache] {
        if (!m_profile)
            return;
        m_profile->clearHttpCache();
        clearCache->setText(tr("Cache cleared"));
        clearCache->setEnabled(false);
    });
    m_clearCacheButton = clearCache;
    dataButtons->addWidget(clearHistory);
    dataButtons->addWidget(clearCookies);
    dataButtons->addWidget(manageCookies);
    dataButtons->addWidget(clearCache);
    dataButtons->addStretch(1);
    layout->addLayout(dataButtons);

    layout->addWidget(sectionTitle(tr("Tor windows")));
    layout->addWidget(mutedLabel(
        tr("A Tor window is a private window whose pages all go through the Tor network: sites don't see "
           "your address, and .onion sites open. Normal and private windows never use Tor. It is not Tor "
           "Browser: sites can still tell this browser apart, so it keeps your address, not your identity, "
           "to yourself. Searches with DuckDuckGo use its onion site.")));
    layout->addWidget(mutedLabel(tr("It uses the Tor already running on this computer, or starts the tor "
                                    "program. %1")
                                     .arg(Tor::installHint())));
    auto *torButton = new QPushButton(Icons::icon(Icons::Private), tr("Open a Tor Window"));
    connect(torButton, &QPushButton::clicked, this, [this] {
        QString error;
        if (!Tor::openWindow({}, &error))
            QMessageBox::warning(this, tr("Tor windows"), error);
    });
    auto *torRow = new QHBoxLayout;
    torRow->addWidget(torButton);
    torRow->addStretch(1);
    layout->addLayout(torRow);

    layout->addWidget(sectionTitle(tr("Built-in protections")));
    layout->addWidget(mutedLabel(
        tr("Always on: websites cannot open local files or javascript: links, no mixed "
           "content, no pop-up windows you did not click for, and no telemetry. Private "
           "windows never write history, cookies or cache to disk.")));

    updateBlockingSummary();
    layout->addStretch(1);
    return page;
}

void SettingsDialog::updateBlockingSummary()
{
    if (!m_blockingSummary)
        return;
    if (!m_adBlocker) {
        m_blockingSummary->setText(tr("Ad blocking is unavailable."));
        return;
    }

    QString text = tr("%1 network rules and %2 cosmetic rules, from your list (%3) and the lists below.")
                       .arg(m_adBlocker->ruleCount())
                       .arg(m_adBlocker->cosmeticRuleCount())
                       .arg(QDir::toNativeSeparators(m_adBlocker->listPath()));
    if (m_adBlocker->isEnabled())
        text += QLatin1Char(' ') + tr("%1 requests blocked so far.").arg(m_adBlocker->blockedCount());
    if (!m_adBlocker->lastListError().isEmpty())
        text += QLatin1Char('\n') + m_adBlocker->lastListError();
    m_blockingSummary->setText(text);
}

QWidget *SettingsDialog::createExtensionsTab()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = pageShell(tr("Extensions"),
                              tr("JavaScript user scripts that run inside pages."), &layout);

    auto *summary = mutedLabel(QString());
    const auto refresh = [this, summary] {
        if (!m_userScripts) {
            summary->setText(tr("Extensions are unavailable."));
            return;
        }
        if (m_userScripts->directory().isEmpty()) {
            summary->setText(tr("Private windows run no user scripts. Open a normal window to "
                                "manage extensions."));
            return;
        }
        int enabled = 0;
        for (const UserScript &script : m_userScripts->scripts())
            enabled += script.enabled ? 1 : 0;
        summary->setText(tr("%1 installed, %2 enabled. Scripts live in %3")
                             .arg(m_userScripts->scripts().size())
                             .arg(enabled)
                             .arg(QDir::toNativeSeparators(m_userScripts->directory())));
    };
    refresh();
    if (m_userScripts)
        connect(m_userScripts, &UserScripts::changed, summary, refresh);
    layout->addWidget(summary);

    auto *buttons = new QHBoxLayout;
    auto *manage = new QPushButton(Icons::icon(Icons::Extensions), tr("Manage extensions…"));
    manage->setProperty("accent", true);
    manage->setEnabled(m_userScripts != nullptr);
    connect(manage, &QPushButton::clicked, this, &SettingsDialog::extensionsRequested);
    buttons->addWidget(manage);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    layout->addWidget(sectionTitle(tr("Writing a script")));
    auto *example = new QLabel(QStringLiteral(
        "<pre>// @name Readability helper\n"
        "// @match *://example.com/*\n"
        "console.log('running on', location.hostname);</pre>"));
    example->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(example);
    layout->addWidget(mutedLabel(tr("Chrome extensions are not supported: QtWebEngine cannot "
                                    "load them.")));
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createDownloadsTab()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = pageShell(tr("Downloads"), tr("Where downloaded files are saved."), &layout);

    layout->addWidget(sectionTitle(tr("Save files to")));
    auto *row = new QHBoxLayout;
    auto *location = new QLineEdit(QDir::toNativeSeparators(
        Preferences::instance()->downloadDirectory()));
    location->setReadOnly(true);
    auto *browse = new QPushButton(Icons::icon(Icons::Folder), tr("Change…"));
    connect(browse, &QPushButton::clicked, this, [this, location] {
        const QString directory = QFileDialog::getExistingDirectory(
            this, tr("Save downloads to"), Preferences::instance()->downloadDirectory());
        if (directory.isEmpty())
            return;
        Preferences::instance()->setDownloadDirectory(directory);
        location->setText(QDir::toNativeSeparators(directory));
    });
    auto *open = new QPushButton(Icons::icon(Icons::Open), tr("Open"));
    connect(open, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(Preferences::instance()->downloadDirectory()));
    });
    row->addWidget(location, 1);
    row->addWidget(browse);
    row->addWidget(open);
    layout->addLayout(row);
    layout->addWidget(mutedLabel(tr("Files are saved without asking. If a file with the same "
                                    "name exists, the new one gets a number added.")));
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createAboutTab()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = pageShell(QString(), QString(), &layout);

    auto *hero = new QHBoxLayout;
    auto *logo = new QLabel;
    logo->setPixmap(Icons::appIcon().pixmap(72, 72));
    hero->addWidget(logo);
    auto *heroText = new QVBoxLayout;
    auto *name = new QLabel(QStringLiteral("SpacePenguin"));
    name->setObjectName(QStringLiteral("heroTitle"));
    heroText->addWidget(name);
    heroText->addWidget(mutedLabel(tr("Version %1 · Qt %2 · %3")
                                       .arg(QCoreApplication::applicationVersion(),
                                            QString::fromLatin1(qVersion()),
                                            QStringLiteral(SPACEPENGUIN_BUILD_TYPE).isEmpty()
                                                ? QStringLiteral("Default")
                                                : QStringLiteral(SPACEPENGUIN_BUILD_TYPE))));
    hero->addLayout(heroText, 1);
    layout->addLayout(hero);
    layout->addSpacing(8);

    auto *description = new QLabel(
        tr("A secure, lightweight, and usable browser written in Qt. Pages are rendered by "
           "Chromium through QtWebEngine. No telemetry, no background services, no "
           "auto-updater."));
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *links = new QLabel(
        QStringLiteral("<a href=\"%1\">%2</a> &nbsp;·&nbsp; "
                       "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">%3</a>")
            .arg(kRepositoryUrl, tr("Source code"), tr("GNU GPL v3")));
    links->setOpenExternalLinks(true);
    layout->addWidget(links);
    layout->addWidget(mutedLabel(tr("Type about:about in the address bar for every internal "
                                    "page, and about:version for build details.")));
    layout->addStretch(1);
    return page;
}

namespace {

/** A list edited one entry a line, saved a moment after the typing stops. */
QPlainTextEdit *lineListEdit(const QStringList &lines, const QString &placeholder, QObject *owner,
                             std::function<void(const QStringList &)> save)
{
    auto *edit = new QPlainTextEdit(lines.join(QLatin1Char('\n')));
    edit->setPlaceholderText(placeholder);
    edit->setFixedHeight(110);
    edit->setTabChangesFocus(true);
    auto *timer = new QTimer(edit);
    timer->setSingleShot(true);
    timer->setInterval(500);
    QObject::connect(edit, &QPlainTextEdit::textChanged, timer, qOverload<>(&QTimer::start));
    QObject::connect(timer, &QTimer::timeout, owner, [edit, save] {
        QStringList entries;
        for (const QString &line : edit->toPlainText().split(QLatin1Char('\n'))) {
            if (!line.trimmed().isEmpty())
                entries.append(line.trimmed());
        }
        save(entries);
    });
    return edit;
}


/** Student mode: joining the teacher's class, and where it stands. */
void addStudentClass(QVBoxLayout *layout, QWidget *owner)
{
    Classroom *classroom = Classroom::instance();
    auto *status = new QLabel;
    status->setWordWrap(true);
    status->setTextFormat(Qt::RichText);
    layout->addWidget(status);

    auto *joinBox = new QWidget;
    auto *join = new QHBoxLayout(joinBox);
    join->setContentsMargins(0, 0, 0, 0);
    auto *name = new QLineEdit(classroom->studentName());
    name->setPlaceholderText(SettingsDialog::tr("Your name"));
    name->setMaxLength(40);
    auto *code = new QLineEdit;
    code->setPlaceholderText(SettingsDialog::tr("Class code"));
    auto *joinButton = new QPushButton(SettingsDialog::tr("Join Class"));
    join->addWidget(name, 1);
    join->addWidget(code, 1);
    join->addWidget(joinButton);
    layout->addWidget(joinBox);
    auto *leaveButton = new QPushButton(SettingsDialog::tr("Leave Class"));
    auto *leaveRow = new QHBoxLayout;
    leaveRow->addWidget(leaveButton);
    leaveRow->addStretch(1);
    layout->addLayout(leaveRow);
    auto *hint = mutedLabel(SettingsDialog::tr("Your teacher gives the code. If the class isn't found, they "
                                               "can give it as code@address."));
    layout->addWidget(hint);

    const auto update = [classroom, status, joinBox, leaveButton, hint] {
        const bool in = classroom->inClass();
        joinBox->setVisible(!in);
        hint->setVisible(!in);
        leaveButton->setVisible(in);
        if (!in) {
            status->setText(SettingsDialog::tr("Not in a class."));
            return;
        }
        status->setText(SettingsDialog::tr("In <b>%1</b> as %2. %3 %4")
                            .arg(classroom->joinedClassName().toHtmlEscaped(), classroom->studentName().toHtmlEscaped(),
                                 SettingsDialog::tr("%n site(s) blocked by your teacher.", nullptr,
                                                    classroom->joinedSiteCount()),
                                 classroom->teacherReachable()
                                     ? SettingsDialog::tr("Connected.")
                                     : SettingsDialog::tr("The teacher's computer isn't answering; the last list "
                                                          "still applies.")));
    };
    update();
    QObject::connect(classroom, &Classroom::changed, status, update);
    QObject::connect(joinButton, &QPushButton::clicked, owner, [classroom, name, code, joinButton] {
        if (name->text().trimmed().isEmpty()) {
            name->setFocus();
            return;
        }
        joinButton->setEnabled(false);
        joinButton->setText(SettingsDialog::tr("Looking for the class…"));
        classroom->join(code->text(), name->text());
    });
    QObject::connect(classroom, &Classroom::joinFinished, joinButton, [owner, joinButton, update](bool ok, const QString &message) {
        joinButton->setEnabled(true);
        joinButton->setText(SettingsDialog::tr("Join Class"));
        if (!ok)
            QMessageBox::warning(owner, SettingsDialog::tr("Join Class"), message);
        update();
    });
    QObject::connect(leaveButton, &QPushButton::clicked, owner, [owner, classroom] {
        if (QMessageBox::question(owner, SettingsDialog::tr("Leave Class"),
                                  SettingsDialog::tr("Leave %1? Your teacher will see you are no longer in the class.")
                                      .arg(classroom->joinedClassName()))
            == QMessageBox::Yes)
            classroom->leave();
    });
}

/** Teacher mode: the class, its blocked sites, its students and their requests. */
void addTeacherClass(QVBoxLayout *layout, QWidget *owner)
{
    Classroom *classroom = Classroom::instance();
    auto *open = new QCheckBox(SettingsDialog::tr("Class is open: students on this network can join"));
    open->setChecked(classroom->classOpen());
    QObject::connect(open, &QCheckBox::toggled, owner, [classroom](bool on) { classroom->setClassOpen(on); });
    layout->addWidget(open);

    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(SettingsDialog::tr("Class name")));
    auto *name = new QLineEdit(classroom->className());
    name->setMaxLength(60);
    QObject::connect(name, &QLineEdit::editingFinished, owner, [classroom, name] { classroom->setClassName(name->text()); });
    nameRow->addWidget(name, 1);
    layout->addLayout(nameRow);

    auto *codeRow = new QHBoxLayout;
    auto *codeLabel = new QLabel;
    codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    codeLabel->setTextFormat(Qt::RichText);
    auto *newCode = new QPushButton(SettingsDialog::tr("New Code"));
    QObject::connect(newCode, &QPushButton::clicked, owner, [owner, classroom] {
        if (confirm(owner, SettingsDialog::tr("New Code"),
                    SettingsDialog::tr("Students who joined with the old code will have to join again.")))
            classroom->newCode();
    });
    codeRow->addWidget(codeLabel, 1);
    codeRow->addWidget(newCode);
    layout->addLayout(codeRow);
    auto *where = mutedLabel(QString());
    where->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(where);

    // The blocked sites, with the reason students see.
    auto *table = new QTableWidget(0, 2);
    table->setHorizontalHeaderLabels({SettingsDialog::tr("Site"), SettingsDialog::tr("Reason students see")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->hide();
    table->setMinimumHeight(180);
    layout->addWidget(table);
    auto *tableButtons = new QHBoxLayout;
    auto *add = new QPushButton(Icons::icon(Icons::Plus), SettingsDialog::tr("Block a Site"));
    auto *remove = new QPushButton(Icons::icon(Icons::Trash), SettingsDialog::tr("Remove"));
    auto *count = new QLabel;
    count->setProperty("muted", true);
    tableButtons->addWidget(add);
    tableButtons->addWidget(remove);
    tableButtons->addStretch(1);
    tableButtons->addWidget(count);
    layout->addLayout(tableButtons);

    auto *students = new QLabel;
    students->setWordWrap(true);
    layout->addWidget(students);
    auto *requests = new QListWidget;
    requests->setMaximumHeight(110);
    layout->addWidget(requests);
    auto *answerRow = new QHBoxLayout;
    auto *allowOne = new QPushButton(SettingsDialog::tr("Allow for Student"));
    auto *allowAll = new QPushButton(SettingsDialog::tr("Allow for Everyone"));
    auto *decline = new QPushButton(SettingsDialog::tr("Decline"));
    answerRow->addWidget(allowOne);
    answerRow->addWidget(allowAll);
    answerRow->addWidget(decline);
    answerRow->addStretch(1);
    layout->addLayout(answerRow);
    layout->addWidget(mutedLabel(SettingsDialog::tr(
        "Only students who join are affected, and only on the sites you list; your own browsing isn't "
        "restricted. This is a classroom tool, not device management: students can leave the class, and "
        "you see who is in it.")));

    auto saving = std::make_shared<bool>(false);
    const auto fillTable = [classroom, table, count, remove, add, saving] {
        if (*saving)
            return;
        const QSignalBlocker blocker(table);
        const QList<BlockedSite> sites = classroom->blockedSites();
        table->setRowCount(int(sites.size()));
        for (int row = 0; row < sites.size(); ++row) {
            table->setItem(row, 0, new QTableWidgetItem(sites.at(row).site));
            table->setItem(row, 1, new QTableWidgetItem(sites.at(row).reason));
        }
        count->setText(SettingsDialog::tr("%1 of %2").arg(sites.size()).arg(Classroom::kMaxSites));
        add->setEnabled(sites.size() < Classroom::kMaxSites);
        remove->setEnabled(!sites.isEmpty());
    };
    const auto saveTable = [classroom, table, saving] {
        QList<BlockedSite> sites;
        for (int row = 0; row < table->rowCount(); ++row) {
            const QTableWidgetItem *site = table->item(row, 0);
            const QTableWidgetItem *reason = table->item(row, 1);
            if (site && !site->text().trimmed().isEmpty())
                sites.append({site->text(), reason ? reason->text() : QString()});
        }
        *saving = true;
        classroom->setBlockedSites(sites);
        *saving = false;
    };
    QObject::connect(table, &QTableWidget::itemChanged, owner, [saveTable, fillTable](QTableWidgetItem *item) {
        saveTable();
        // A site typed as an address is shown as it was kept: "youtube.com".
        if (item->column() == 0)
            QTimer::singleShot(0, item->tableWidget(), fillTable);
    });
    QObject::connect(add, &QPushButton::clicked, owner, [table] {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 1, new QTableWidgetItem);
        auto *site = new QTableWidgetItem;
        table->setItem(row, 0, site);
        table->setCurrentItem(site);
        table->editItem(site);
    });
    QObject::connect(remove, &QPushButton::clicked, owner, [table, saveTable, fillTable] {
        const int row = table->currentRow();
        if (row < 0)
            return;
        table->removeRow(row);
        saveTable();
        fillTable();
    });

    const auto selectedRequest = [requests] {
        const QListWidgetItem *item = requests->currentItem();
        return item ? item->data(Qt::UserRole).toString() : QString();
    };
    QObject::connect(allowOne, &QPushButton::clicked, owner, [classroom, selectedRequest] {
        classroom->answer(selectedRequest(), Classroom::Answer::AllowStudent);
    });
    QObject::connect(allowAll, &QPushButton::clicked, owner, [classroom, selectedRequest] {
        classroom->answer(selectedRequest(), Classroom::Answer::AllowEveryone);
    });
    QObject::connect(decline, &QPushButton::clicked, owner, [classroom, selectedRequest] {
        classroom->answer(selectedRequest(), Classroom::Answer::Decline);
    });

    const auto update = [classroom, codeLabel, where, newCode, students, requests, allowOne, allowAll, decline, fillTable] {
        const bool open = classroom->classOpen();
        newCode->setEnabled(open);
        if (!open) {
            codeLabel->setText(SettingsDialog::tr("Open the class to get its code."));
            where->clear();
        } else {
            codeLabel->setText(SettingsDialog::tr("Class code: <b style=\"font-size:16pt;letter-spacing:2px\">%1</b>")
                                   .arg(classroom->code()));
            QStringList manual;
            for (const QString &address : Classroom::localAddresses())
                manual.append(classroom->port() && classroom->port() != Classroom::kHttpPort
                                  ? QStringLiteral("%1@%2:%3").arg(classroom->code(), address).arg(classroom->port())
                                  : QStringLiteral("%1@%2").arg(classroom->code(), address));
            where->setText(!classroom->serving()
                               ? SettingsDialog::tr("The class could not open on this computer's network ports.")
                               : manual.isEmpty() ? SettingsDialog::tr("This computer is not on a network.")
                                                  : SettingsDialog::tr("Students type the code. Where the network "
                                                                       "keeps devices apart, they type %1.")
                                                        .arg(manual.join(SettingsDialog::tr(" or "))));
        }
        QStringList names;
        const QDateTime now = QDateTime::currentDateTime();
        for (const ClassStudent &student : classroom->students()) {
            const qint64 minutes = student.lastSeen.secsTo(now) / 60;
            names.append(minutes < 1 ? student.name
                                     : SettingsDialog::tr("%1 (%2 min ago)").arg(student.name).arg(minutes));
        }
        students->setText(names.isEmpty() ? SettingsDialog::tr("No students have joined yet.")
                                          : SettingsDialog::tr("Students: %1").arg(names.join(QStringLiteral(", "))));
        requests->clear();
        for (const UnblockRequest &request : classroom->requests()) {
            auto *item = new QListWidgetItem(
                request.note.isEmpty() ? SettingsDialog::tr("%1 asks for %2").arg(request.student, request.site)
                                       : SettingsDialog::tr("%1 asks for %2: “%3”").arg(request.student, request.site, request.note),
                requests);
            item->setData(Qt::UserRole, request.id);
        }
        if (requests->count() == 0)
            new QListWidgetItem(SettingsDialog::tr("No requests to unblock."), requests);
        else
            requests->setCurrentRow(0);
        const bool any = !classroom->requests().isEmpty();
        allowOne->setEnabled(any);
        allowAll->setEnabled(any);
        decline->setEnabled(any);
        fillTable();
    };
    update();
    QObject::connect(classroom, &Classroom::changed, codeLabel, update);
}
} // namespace

QWidget *SettingsDialog::createRolesTab()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = pageShell(tr("Made for you"),
                              tr("Tell SpacePenguin what you use it for, and it adds the settings and tools "
                                 "for that. Pick as many as fit; everything else stays the same."),
                              &layout);
    Preferences *preferences = Preferences::instance();

    layout->addWidget(sectionTitle(tr("I use SpacePenguin as a…")));
    QMap<Roles::Role, QCheckBox *> boxes;
    for (const Roles::Role role : Roles::all()) {
        auto *box = new QCheckBox(Roles::label(role));
        box->setChecked(preferences->hasRole(Roles::key(role)));
        layout->addWidget(box);
        auto *description = mutedLabel(Roles::description(role));
        description->setContentsMargins(26, 0, 0, 4);
        layout->addWidget(description);
        boxes.insert(role, box);
    }

    // Each role's settings, shown while it is picked.
    const auto panel = [layout](const QString &title, QVBoxLayout **box) {
        auto *widget = new QWidget;
        *box = new QVBoxLayout(widget);
        (*box)->setContentsMargins(0, 8, 0, 0);
        (*box)->setSpacing(8);
        (*box)->addWidget(sectionTitle(title));
        layout->addWidget(widget);
        return widget;
    };

    QVBoxLayout *developerLayout = nullptr;
    QWidget *developer = panel(tr("Developer"), &developerLayout);
    auto *noCache = new QCheckBox(tr("Load every page fresh, without the cache"));
    noCache->setChecked(preferences->bypassCache());
    connect(noCache, &QCheckBox::toggled, this, [](bool on) { Preferences::instance()->setBypassCache(on); });
    developerLayout->addWidget(noCache);
    auto *userAgent = new QLineEdit(preferences->userAgent());
    userAgent->setPlaceholderText(tr("User agent (empty: the browser's own)"));
    userAgent->setClearButtonEnabled(true);
    connect(userAgent, &QLineEdit::editingFinished, this,
            [userAgent] { Preferences::instance()->setUserAgent(userAgent->text()); });
    developerLayout->addWidget(userAgent);
    developerLayout->addWidget(mutedLabel(tr("F12 or Ctrl+Shift+I opens the developer tools for the page; "
                                             "Ctrl+U shows its source. Both are in the menu too.")));

    QVBoxLayout *gamerLayout = nullptr;
    QWidget *gamer = panel(tr("Gamer"), &gamerLayout);
    auto *pointer = new QCheckBox(tr("Let games lock the mouse pointer without asking"));
    pointer->setChecked(preferences->allowPointerLock());
    connect(pointer, &QCheckBox::toggled, this, [](bool on) { Preferences::instance()->setAllowPointerLock(on); });
    gamerLayout->addWidget(pointer);
    gamerLayout->addWidget(mutedLabel(tr("Esc always gives the pointer back.")));
    auto *mute = new QCheckBox(tr("Mute the other tabs while a game is full screen"));
    mute->setChecked(preferences->muteOthersInFullScreen());
    connect(mute, &QCheckBox::toggled, this,
            [](bool on) { Preferences::instance()->setMuteOthersInFullScreen(on); });
    gamerLayout->addWidget(mute);

    QVBoxLayout *focusLayout = nullptr;
    QWidget *focus = panel(tr("Focus mode"), &focusLayout);
    auto *focusOn = new QCheckBox(tr("Focus mode is on"));
    focusOn->setChecked(preferences->focusMode());
    connect(focusOn, &QCheckBox::toggled, this, [](bool on) { Preferences::instance()->setFocusMode(on); });
    focusLayout->addWidget(focusOn);
    focusLayout->addWidget(mutedLabel(tr("While it is on, these sites show a reminder instead of the page. "
                                         "One site a line; its subdomains count too. Turn it on and off "
                                         "from the menu.")));
    focusLayout->addWidget(lineListEdit(preferences->focusSites(), QStringLiteral("youtube.com"), this,
                                        [](const QStringList &sites) {
                                            QStringList clean;
                                            for (const QString &site : sites) {
                                                const QString normal = Roles::normalizeSite(site);
                                                if (!normal.isEmpty() && !clean.contains(normal))
                                                    clean.append(normal);
                                            }
                                            Preferences::instance()->setFocusSites(clean);
                                        }));

    QVBoxLayout *studentLayout = nullptr;
    QWidget *student = panel(tr("Student"), &studentLayout);
    addStudentClass(studentLayout, this);
    studentLayout->addWidget(mutedLabel(tr("Copy Citation (Ctrl+Alt+C, or in the menu) puts a reference "
                                           "to the page on the clipboard: its title, site, address and "
                                           "today's date.")));

    QVBoxLayout *workerLayout = nullptr;
    QWidget *worker = panel(tr("Worker"), &workerLayout);
    workerLayout->addWidget(mutedLabel(tr("Your work pages, one address a line. Open Work Pages in the menu "
                                          "opens them all.")));
    workerLayout->addWidget(lineListEdit(preferences->workPages(), QStringLiteral("https://mail.example.com"),
                                         this, [](const QStringList &pages) {
                                             Preferences::instance()->setWorkPages(pages);
                                         }));
    auto *onStart = new QCheckBox(tr("Open them when SpacePenguin starts"));
    onStart->setChecked(preferences->openWorkPagesOnStart());
    connect(onStart, &QCheckBox::toggled, this,
            [](bool on) { Preferences::instance()->setOpenWorkPagesOnStart(on); });
    workerLayout->addWidget(onStart);

    QVBoxLayout *teacherLayout = nullptr;
    QWidget *teacher = panel(tr("Teacher"), &teacherLayout);
    addTeacherClass(teacherLayout, this);
    auto *zoomRow = new QHBoxLayout;
    zoomRow->addWidget(new QLabel(tr("Pages open at")));
    auto *zoom = new QComboBox;
    for (const int percent : {100, 110, 125, 150, 175, 200})
        zoom->addItem(QStringLiteral("%1%").arg(percent), percent);
    zoom->setCurrentIndex(qMax(0, zoom->findData(preferences->pageZoom())));
    connect(zoom, &QComboBox::currentIndexChanged, this, [zoom] {
        Preferences::instance()->setPageZoom(zoom->currentData().toInt());
    });
    zoomRow->addWidget(zoom);
    zoomRow->addStretch(1);
    teacherLayout->addLayout(zoomRow);
    teacherLayout->addWidget(mutedLabel(tr("A larger size reads well on a projector; Ctrl+0 goes back to it.")));

    const auto sync = [boxes, developer, gamer, focus, student, worker, teacher] {
        QStringList roles;
        for (auto it = boxes.cbegin(); it != boxes.cend(); ++it) {
            if (it.value()->isChecked())
                roles.append(Roles::key(it.key()));
        }
        // In the order of Roles::all(), whatever order they were ticked in.
        QStringList ordered;
        for (const Roles::Role role : Roles::all()) {
            if (roles.contains(Roles::key(role)))
                ordered.append(Roles::key(role));
        }
        developer->setVisible(ordered.contains(Roles::key(Roles::Role::Developer)));
        gamer->setVisible(ordered.contains(Roles::key(Roles::Role::Gamer)));
        focus->setVisible(Roles::hasFocusMode(ordered));
        student->setVisible(ordered.contains(Roles::key(Roles::Role::Student)));
        worker->setVisible(ordered.contains(Roles::key(Roles::Role::Worker)));
        teacher->setVisible(ordered.contains(Roles::key(Roles::Role::Teacher)));
        return ordered;
    };
    for (QCheckBox *box : boxes)
        connect(box, &QCheckBox::toggled, this, [sync] { Preferences::instance()->setRoles(sync()); });
    sync();

    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createAppsTab()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = pageShell(tr("Apps"),
                              tr("Sites installed as apps, each in a window of its own with its own icon. "
                                 "To add one, open the site and choose Install as App in the menu."),
                              &layout);

    auto *list = new QListWidget;
    list->setIconSize(QSize(32, 32));
    list->setMinimumHeight(220);
    layout->addWidget(list);

    auto *buttons = new QHBoxLayout;
    auto *openButton = new QPushButton(tr("Open"));
    auto *removeButton = new QPushButton(Icons::icon(Icons::Trash), tr("Remove"));
    buttons->addWidget(openButton);
    buttons->addWidget(removeButton);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    layout->addWidget(mutedLabel(tr("Removing an app takes away its window and launcher; the site's "
                                    "sign-ins and data stay, as they are the browser's.")));

    const auto rebuild = [list, openButton, removeButton] {
        list->clear();
        const QList<WebApp> apps = WebApps::instance()->apps();
        for (const WebApp &app : apps) {
            const QImage icon = WebApps::instance()->icon(app.id);
            auto *item = new QListWidgetItem(
                icon.isNull() ? Icons::icon(Icons::Globe) : QIcon(QPixmap::fromImage(icon)),
                QStringLiteral("%1\n%2").arg(app.name, app.startUrl.host()), list);
            item->setData(Qt::UserRole, app.id);
        }
        if (apps.isEmpty()) {
            auto *empty = new QListWidgetItem(tr("No apps yet."), list);
            empty->setFlags(Qt::NoItemFlags);
        }
        openButton->setEnabled(!apps.isEmpty());
        removeButton->setEnabled(!apps.isEmpty());
        if (!apps.isEmpty())
            list->setCurrentRow(0);
    };
    rebuild();
    connect(WebApps::instance(), &WebApps::changed, list, rebuild);

    const auto selected = [list] {
        const QListWidgetItem *item = list->currentItem();
        return item ? item->data(Qt::UserRole).toString() : QString();
    };
    connect(openButton, &QPushButton::clicked, this, [this, selected] {
        if (!selected().isEmpty())
            emit appOpenRequested(selected());
    });
    connect(list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        const QString id = item->data(Qt::UserRole).toString();
        if (!id.isEmpty())
            emit appOpenRequested(id);
    });
    connect(removeButton, &QPushButton::clicked, this, [this, selected] {
        const QString id = selected();
        if (id.isEmpty())
            return;
        const WebApp app = WebApps::instance()->app(id);
        if (confirm(this, tr("Remove App"), tr("Remove “%1” and its launcher?").arg(app.name)))
            WebApps::instance()->remove(id);
    });

    layout->addStretch(1);
    return page;
}

}

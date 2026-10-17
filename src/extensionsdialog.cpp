#include "extensionsdialog.h"

#include "chromeextensions.h"
#include "icons.h"
#include "userextensions.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace spacepenguin {

ExtensionsDialog::ExtensionsDialog(UserScripts *scripts, ChromeExtensions *chrome, QWidget *parent)
    : QDialog(parent)
    , m_scripts(scripts)
    , m_chrome(chrome)
{
    setWindowTitle(tr("Extensions"));
    resize(640, 640);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 12);
    layout->setSpacing(10);

    auto *heading = new QLabel(tr("Extensions"), this);
    heading->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(heading);

    // ---- Chrome extensions ----
    auto *chromeTitle = new QLabel(tr("Chrome extensions"), this);
    chromeTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(chromeTitle);
    QString chromeIntro;
    if (!ChromeExtensions::isAvailable())
        chromeIntro = tr("Not yet: Chrome extensions need SpacePenguin built with Qt 6.10 or "
                         "newer. The user scripts below work now.");
    else if (!ChromeExtensions::isSupported())
        chromeIntro = tr("Not yet: in the Qt versions so far (6.10 and 6.11), turning a Chrome "
                         "extension on crashes the browser, so they are switched off until Qt fixes "
                         "it. To try them anyway, start SpacePenguin with "
                         "SPACEPENGUIN_CHROME_EXTENSIONS=1. The user scripts below work now.");
    else if (!m_chrome)
        chromeIntro = tr("Extensions do not run in private windows. Open a normal window to "
                         "add or manage them.");
    else
        chromeIntro = tr("Manifest V3 extensions from the Chrome Web Store, or from a folder, "
                         ".zip or .crx file. On a store page, the bar above the page offers to "
                         "add the extension. Extensions do not run in private windows.");
    auto *chromeLabel = new QLabel(chromeIntro, this);
    chromeLabel->setWordWrap(true);
    chromeLabel->setProperty("muted", true);
    layout->addWidget(chromeLabel);

    m_chromeList = new QListWidget(this);
    m_chromeList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_chromeList->setIconSize(QSize(20, 20));
    m_chromeList->setEnabled(m_chrome != nullptr);
    layout->addWidget(m_chromeList, 1);

    auto *chromeButtons = new QHBoxLayout;
    auto *store = new QPushButton(Icons::icon(Icons::Globe), tr("Chrome Web Store"), this);
    store->setProperty("accent", true);
    connect(store, &QPushButton::clicked, this, [this] {
        emit webStoreRequested();
        accept();
    });
    auto *addFile = new QPushButton(Icons::icon(Icons::Plus), tr("Add file…"), this);
    connect(addFile, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Add a Chrome extension"), QDir::homePath(),
            tr("Chrome extensions (*.crx *.zip)"));
        if (!path.isEmpty())
            m_chrome->install(path);
    });
    auto *addFolder = new QPushButton(Icons::icon(Icons::Folder), tr("Add folder…"), this);
    addFolder->setToolTip(tr("An unpacked extension: a folder with a manifest.json"));
    connect(addFolder, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getExistingDirectory(this, tr("Add an unpacked extension"),
                                                               QDir::homePath());
        if (!path.isEmpty())
            m_chrome->install(path);
    });
    m_chromeToggle = new QPushButton(tr("Disable"), this);
    connect(m_chromeToggle, &QPushButton::clicked, this, [this] {
        const QString id = selectedChromeId();
        for (const ChromeExtensions::Extension &extension : m_chrome->extensions()) {
            if (extension.id == id)
                m_chrome->setEnabled(id, !extension.enabled);
        }
    });
    m_chromeRemove = new QPushButton(Icons::icon(Icons::Trash), tr("Remove"), this);
    connect(m_chromeRemove, &QPushButton::clicked, this, [this] {
        const QString id = selectedChromeId();
        if (!id.isEmpty())
            m_chrome->uninstall(id);
    });
    for (QPushButton *button : {store, addFile, addFolder})
        button->setEnabled(m_chrome != nullptr);
    chromeButtons->addWidget(store);
    chromeButtons->addWidget(addFile);
    chromeButtons->addWidget(addFolder);
    chromeButtons->addStretch(1);
    chromeButtons->addWidget(m_chromeToggle);
    chromeButtons->addWidget(m_chromeRemove);
    layout->addLayout(chromeButtons);

    if (m_chrome) {
        connect(m_chrome, &ChromeExtensions::changed, this, &ExtensionsDialog::reloadChrome);
        connect(m_chrome, &ChromeExtensions::failed, this,
                [this](const QString &message) { QMessageBox::warning(this, tr("Extensions"), message); });
    }
    connect(m_chromeList, &QListWidget::itemSelectionChanged, this, &ExtensionsDialog::reloadChrome);

    // ---- User scripts ----
    auto *scriptsTitle = new QLabel(tr("User scripts"), this);
    scriptsTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(scriptsTitle);
    auto *intro = new QLabel(tr("JavaScript files that run in the pages you choose, with "
                                "Greasemonkey-style headers."),
                             this);
    intro->setWordWrap(true);
    intro->setProperty("muted", true);
    layout->addWidget(intro);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setIconSize(QSize(18, 18));
    layout->addWidget(m_list);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setProperty("muted", true);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_summary);

    auto *buttons = new QHBoxLayout;
    auto *addButton = new QPushButton(Icons::icon(Icons::Plus), tr("Add script…"), this);
    addButton->setProperty("accent", true);
    connect(addButton, &QPushButton::clicked, this, &ExtensionsDialog::addScript);
    buttons->addWidget(addButton);

    m_toggleButton = new QPushButton(tr("Disable"), this);
    connect(m_toggleButton, &QPushButton::clicked, this, [this] {
        const QString id = selectedId();
        if (id.isEmpty() || !m_scripts)
            return;
        m_scripts->setScriptEnabled(id, !m_scripts->isEnabled(id));
        reload();
    });
    buttons->addWidget(m_toggleButton);

    m_removeButton = new QPushButton(Icons::icon(Icons::Trash), tr("Remove"), this);
    connect(m_removeButton, &QPushButton::clicked, this, &ExtensionsDialog::removeSelected);
    buttons->addWidget(m_removeButton);
    buttons->addStretch(1);

    auto *folderButton = new QPushButton(Icons::icon(Icons::Folder), tr("Open folder"), this);
    folderButton->setEnabled(m_scripts && !m_scripts->directory().isEmpty());
    connect(folderButton, &QPushButton::clicked, this, [this] {
        QDir().mkpath(m_scripts->directory());
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_scripts->directory()));
    });
    buttons->addWidget(folderButton);
    layout->addLayout(buttons);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttonBox);

    connect(m_list, &QListWidget::itemSelectionChanged, this,
            &ExtensionsDialog::updateButtons);
    // The checkbox beside each script enables or disables it.
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (!m_scripts)
            return;
        const QString id = item->data(Qt::UserRole).toString();
        const bool enabled = item->checkState() == Qt::Checked;
        if (m_scripts->isEnabled(id) != enabled) {
            m_scripts->setScriptEnabled(id, enabled);
            QMetaObject::invokeMethod(this, &ExtensionsDialog::reload, Qt::QueuedConnection);
        }
    });
    if (m_scripts) {
        connect(m_scripts, &UserScripts::changed, this, &ExtensionsDialog::reload);
    }

    // Enter in the search field must not press a button.
    for (QPushButton *button : findChildren<QPushButton *>())
        button->setAutoDefault(false);

    reload();
    reloadChrome();
}

QString ExtensionsDialog::selectedChromeId() const
{
    const auto items = m_chromeList->selectedItems();
    return items.isEmpty() ? QString() : items.first()->data(Qt::UserRole).toString();
}

void ExtensionsDialog::reloadChrome()
{
    if (!m_chrome) {
        m_chromeToggle->setEnabled(false);
        m_chromeRemove->setEnabled(false);
        return;
    }
    // Rebuilding the list would drop the selection that a click just made.
    if (sender() != m_chromeList) {
        const QString selected = selectedChromeId();
        const QSignalBlocker blocker(m_chromeList);
        m_chromeList->clear();
        for (const ChromeExtensions::Extension &extension : m_chrome->extensions()) {
            QString text = extension.name;
            if (!extension.enabled)
                text += tr("  (off)");
            if (!extension.description.isEmpty())
                text += QStringLiteral("\n") + extension.description;
            if (!extension.error.isEmpty())
                text += QStringLiteral("\n") + extension.error;
            auto *item = new QListWidgetItem(
                extension.icon.isNull() ? Icons::icon(Icons::Extensions) : extension.icon, text, m_chromeList);
            item->setData(Qt::UserRole, extension.id);
            item->setToolTip(QDir::toNativeSeparators(extension.path));
            if (extension.id == selected)
                item->setSelected(true);
        }
        if (m_chromeList->count() == 0) {
            auto *empty = new QListWidgetItem(tr("No Chrome extensions yet."), m_chromeList);
            empty->setFlags(Qt::NoItemFlags);
        }
    }
    const QString id = selectedChromeId();
    bool enabled = false;
    for (const ChromeExtensions::Extension &extension : m_chrome->extensions()) {
        if (extension.id == id)
            enabled = extension.enabled;
    }
    m_chromeToggle->setEnabled(!id.isEmpty());
    m_chromeToggle->setText(enabled || id.isEmpty() ? tr("Disable") : tr("Enable"));
    m_chromeRemove->setEnabled(!id.isEmpty());
}

QString ExtensionsDialog::selectedId() const
{
    const auto items = m_list->selectedItems();
    if (items.isEmpty())
        return QString();
    return items.first()->data(Qt::UserRole).toString();
}

void ExtensionsDialog::reload()
{
    const QString previous = selectedId();
    const QSignalBlocker blocker(m_list);
    m_list->clear();

    if (!m_scripts) {
        m_summary->setText(tr("Extensions are unavailable."));
        updateButtons();
        return;
    }

    const QList<UserScript> scripts = m_scripts->scripts();
    for (const UserScript &script : scripts) {
        auto *item = new QListWidgetItem(Icons::icon(Icons::Extensions), script.name, m_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(script.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, script.id);
        item->setToolTip(script.matchPatterns.isEmpty()
                             ? tr("Runs on every site. No @match patterns.")
                             : tr("Runs on: %1").arg(script.matchPatterns.join(QLatin1String(", "))));
        if (!script.enabled)
            item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));

        if (script.id == previous)
            m_list->setCurrentItem(item);
    }

    if (m_scripts->directory().isEmpty()) {
        m_summary->setText(tr("Extensions are not available in private windows. Open a normal "
                              "window to manage user scripts."));
    } else if (scripts.isEmpty()) {
        m_summary->setText(tr("No extensions installed. Drop a .js file in %1")
                               .arg(QDir::toNativeSeparators(m_scripts->directory())));
    } else {
        m_summary->setText(tr("Drop a .js file in %1 to install it. Files may declare "
                              "// @name and // @match patterns in a leading comment.")
                               .arg(QDir::toNativeSeparators(m_scripts->directory())));
    }

    updateButtons();
}

void ExtensionsDialog::updateButtons()
{
    const QString id = selectedId();
    const bool hasSelection = !id.isEmpty() && m_scripts;
    m_toggleButton->setEnabled(hasSelection);
    m_removeButton->setEnabled(hasSelection);
    m_toggleButton->setText(m_scripts && m_scripts->isEnabled(id) ? tr("Disable") : tr("Enable"));
}

void ExtensionsDialog::addScript()
{
    if (!m_scripts)
        return;

    const QString path = QFileDialog::getOpenFileName(this, tr("Add extension"), QString(),
                                                      tr("JavaScript files (*.js)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!m_scripts->addScript(path, &error)) {
        QMessageBox::warning(this, tr("Cannot add extension"), error);
        return;
    }

    reload();
}

void ExtensionsDialog::removeSelected()
{
    const QString id = selectedId();
    if (id.isEmpty() || !m_scripts)
        return;

    m_scripts->removeScript(id);
    reload();
}

}
#include "extensionsdialog.h"

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

ExtensionsDialog::ExtensionsDialog(UserScripts *scripts, QWidget *parent)
    : QDialog(parent)
    , m_scripts(scripts)
{
    setWindowTitle(tr("Extensions"));
    resize(600, 460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 12);
    layout->setSpacing(10);

    auto *heading = new QLabel(tr("Extensions"), this);
    heading->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(heading);

    auto *intro = new QLabel(
        tr("SpacePenguin runs JavaScript extensions inside every page. These are user "
           "scripts, not Chrome extensions — Chrome extension support does not exist in "
           "QtWebEngine and is not planned here."),
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
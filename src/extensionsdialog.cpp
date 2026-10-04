#include "extensionsdialog.h"

#include "userextensions.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace spacepenguin {

ExtensionsDialog::ExtensionsDialog(UserScripts *scripts, QWidget *parent)
    : QDialog(parent)
    , m_scripts(scripts)
{
    setWindowTitle(tr("Extensions"));
    resize(560, 420);

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(
        tr("SpacePenguin runs JavaScript extensions inside every page. These are user "
           "scripts, not Chrome extensions — Chrome extension support does not exist in "
           "QtWebEngine and is not planned here."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    auto *buttons = new QHBoxLayout;
    auto *addButton = new QPushButton(tr("Add script…"), this);
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

    m_removeButton = new QPushButton(tr("Remove"), this);
    connect(m_removeButton, &QPushButton::clicked, this, &ExtensionsDialog::removeSelected);
    buttons->addWidget(m_removeButton);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttonBox);

    connect(m_list, &QListWidget::itemSelectionChanged, this,
            &ExtensionsDialog::updateButtons);
    if (m_scripts) {
        connect(m_scripts, &UserScripts::changed, this, &ExtensionsDialog::reload);
    }

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
    m_list->clear();

    if (!m_scripts) {
        m_summary->setText(tr("Extensions are unavailable."));
        updateButtons();
        return;
    }

    const QList<UserScript> scripts = m_scripts->scripts();
    for (const UserScript &script : scripts) {
        auto *item = new QListWidgetItem(
            tr("%1 — %2").arg(script.name, script.enabled ? tr("enabled") : tr("disabled")),
            m_list);
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
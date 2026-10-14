#include "bookmarksdialog.h"

#include "bookmarkmanager.h"
#include "icons.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace spacepenguin {

// ---- BookmarkEditDialog ---------------------------------------------------------

BookmarkEditDialog::BookmarkEditDialog(const QString &title, const QUrl &url, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Bookmark"));
    setMinimumWidth(440);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 14);
    layout->setSpacing(12);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_title = new QLineEdit(title);
    m_url = new QLineEdit(url.toDisplayString());
    form->addRow(tr("Name"), m_title);
    form->addRow(tr("Address"), m_url);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    QPushButton *remove = buttons->addButton(tr("Remove"), QDialogButtonBox::DestructiveRole);
    remove->setProperty("danger", true);
    connect(remove, &QPushButton::clicked, this, [this] {
        m_remove = true;
        accept();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QPushButton *save = buttons->button(QDialogButtonBox::Save);
    const auto validate = [this, save] { save->setEnabled(this->url().isValid() && !this->url().isEmpty()); };
    connect(m_url, &QLineEdit::textChanged, this, validate);
    validate();

    layout->addWidget(buttons);
    m_title->setFocus();
    m_title->selectAll();
}

QString BookmarkEditDialog::title() const
{
    return m_title->text().trimmed();
}

QUrl BookmarkEditDialog::url() const
{
    return QUrl::fromUserInput(m_url->text().trimmed());
}

void BookmarkEditDialog::editBookmark(BookmarkManager *bookmarks, const QString &id, QWidget *parent)
{
    if (!bookmarks)
        return;

    for (const Bookmark &bookmark : bookmarks->bookmarks()) {
        if (bookmark.id != id)
            continue;

        BookmarkEditDialog dialog(bookmark.title, bookmark.url, parent);
        if (dialog.exec() != QDialog::Accepted)
            return;
        if (dialog.removeRequested()) {
            bookmarks->removeBookmark(id);
            return;
        }
        Bookmark updated = bookmark;
        updated.title = dialog.title();
        updated.url = dialog.url();
        bookmarks->updateBookmark(updated);
        return;
    }
}

// ---- BookmarksDialog ------------------------------------------------------------

BookmarksDialog::BookmarksDialog(BookmarkManager *bookmarks, QWidget *parent)
    : QDialog(parent)
    , m_bookmarks(bookmarks)
{
    setWindowTitle(tr("Bookmarks"));
    resize(720, 520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(10);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search bookmarks"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(Icons::icon(Icons::Search), QLineEdit::LeadingPosition);
    layout->addWidget(m_search);

    m_tree = new QTreeWidget;
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Name"), tr("Address")});
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(m_tree, 1);

    auto *buttons = new QHBoxLayout;
    m_openButton = new QPushButton(Icons::icon(Icons::Open), tr("Open"));
    m_editButton = new QPushButton(tr("Edit…"));
    m_removeButton = new QPushButton(Icons::icon(Icons::Trash), tr("Delete"));
    buttons->addWidget(m_openButton);
    buttons->addWidget(m_editButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch(1);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    const auto open = [this] {
        for (QTreeWidgetItem *item : m_tree->selectedItems())
            emit openRequested(item->data(1, Qt::UserRole).toUrl());
    };
    connect(m_tree, &QTreeWidget::itemActivated, this, open);
    connect(m_openButton, &QPushButton::clicked, this, open);
    connect(m_editButton, &QPushButton::clicked, this,
            [this] { BookmarkEditDialog::editBookmark(m_bookmarks, selectedId(), this); });
    connect(m_removeButton, &QPushButton::clicked, this,
            [this] { m_bookmarks->removeBookmark(selectedId()); });
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &BookmarksDialog::updateButtons);
    connect(m_search, &QLineEdit::textChanged, this, &BookmarksDialog::reload);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (m_bookmarks) {
        connect(m_bookmarks, &BookmarkManager::bookmarkAdded, this, &BookmarksDialog::reload);
        connect(m_bookmarks, &BookmarkManager::bookmarkRemoved, this, &BookmarksDialog::reload);
        connect(m_bookmarks, &BookmarkManager::bookmarkUpdated, this, &BookmarksDialog::reload);
    }

    // Enter in the search field must not press a button.
    for (QPushButton *button : findChildren<QPushButton *>())
        button->setAutoDefault(false);

    reload();
}

void BookmarksDialog::reload()
{
    const QString previous = selectedId();
    m_tree->clear();
    if (!m_bookmarks)
        return;

    const QString search = m_search->text().trimmed();
    for (const Bookmark &bookmark : m_bookmarks->bookmarks()) {
        if (!bookmark.url.isValid() || bookmark.url.isEmpty())
            continue; // folders
        if (!search.isEmpty() && !bookmark.title.contains(search, Qt::CaseInsensitive)
            && !bookmark.url.toString().contains(search, Qt::CaseInsensitive)) {
            continue;
        }
        auto *item = new QTreeWidgetItem(m_tree);
        item->setIcon(0, Icons::icon(Icons::Star));
        item->setText(0, bookmark.title.isEmpty() ? bookmark.url.host() : bookmark.title);
        item->setText(1, bookmark.url.toDisplayString());
        item->setData(0, Qt::UserRole, bookmark.id);
        item->setData(1, Qt::UserRole, bookmark.url);
        if (bookmark.id == previous)
            m_tree->setCurrentItem(item);
    }

    if (m_tree->topLevelItemCount() == 0) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, search.isEmpty() ? tr("No bookmarks yet — press Ctrl+D on a page.")
                                          : tr("No bookmarks match “%1”.").arg(search));
        item->setFlags(Qt::NoItemFlags);
        item->setFirstColumnSpanned(true);
    }
    updateButtons();
}

QString BookmarksDialog::selectedId() const
{
    const auto items = m_tree->selectedItems();
    return items.isEmpty() ? QString() : items.first()->data(0, Qt::UserRole).toString();
}

void BookmarksDialog::updateButtons()
{
    const bool selection = !selectedId().isEmpty();
    m_openButton->setEnabled(selection);
    m_editButton->setEnabled(selection);
    m_removeButton->setEnabled(selection);
}

}

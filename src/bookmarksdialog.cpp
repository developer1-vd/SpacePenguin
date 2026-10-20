#include "bookmarksdialog.h"

#include "bookmarkmanager.h"
#include "icons.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QCursor>
#include <QFileInfo>

#include <functional>

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

    m_form = new QFormLayout;
    m_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_title = new QLineEdit(title);
    m_url = new QLineEdit(url.toDisplayString());
    m_form->addRow(tr("Name"), m_title);
    m_form->addRow(tr("Address"), m_url);
    layout->addLayout(m_form);

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
    m_save = save;
    const auto validate = [this, save] {
        save->setEnabled(!m_url->isVisible() ? !m_title->text().trimmed().isEmpty()
                                            : this->url().isValid() && !this->url().isEmpty());
    };
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

void BookmarkEditDialog::setFolders(const QList<QPair<QString, QString>> &folders, const QString &current)
{
    m_folder = new QComboBox;
    m_folder->addItem(Icons::icon(Icons::Bookmarks), tr("Bookmarks bar"), QString());
    for (const auto &folder : folders)
        m_folder->addItem(Icons::icon(Icons::Folder), folder.second, folder.first);
    m_folder->setCurrentIndex(qMax(0, m_folder->findData(current)));
    m_form->addRow(tr("Folder"), m_folder);
}

QString BookmarkEditDialog::folderId() const
{
    return m_folder ? m_folder->currentData().toString() : QString();
}

void BookmarkEditDialog::setFolderMode()
{
    setWindowTitle(tr("Edit Folder"));
    m_form->setRowVisible(m_url, false);
    m_url->hide();
    m_save->setEnabled(!m_title->text().trimmed().isEmpty());
    connect(m_title, &QLineEdit::textChanged, this,
            [this](const QString &text) { m_save->setEnabled(!text.trimmed().isEmpty()); });
}

void BookmarkEditDialog::editBookmark(BookmarkManager *bookmarks, const QString &id, QWidget *parent)
{
    if (!bookmarks)
        return;

    for (const Bookmark &bookmark : bookmarks->bookmarks()) {
        if (bookmark.id != id)
            continue;

        const bool folder = BookmarkManager::isFolder(bookmark);
        BookmarkEditDialog dialog(bookmark.title, bookmark.url, parent);
        // A folder cannot go inside itself.
        QList<QPair<QString, QString>> folders = bookmarks->folderPaths();
        QString ownPath;
        for (const auto &entry : folders) {
            if (entry.first == id)
                ownPath = entry.second;
        }
        folders.removeIf([&](const QPair<QString, QString> &entry) {
            return folder && (entry.first == id || entry.second.startsWith(ownPath + QStringLiteral(" › ")));
        });
        dialog.setFolders(folders, bookmark.folderId);
        if (folder)
            dialog.setFolderMode();
        if (dialog.exec() != QDialog::Accepted)
            return;
        if (dialog.removeRequested()) {
            const int inside = int(bookmarks->folderBookmarks(id).size());
            if (folder && inside > 0
                && QMessageBox::question(parent, tr("Remove folder"),
                                         tr("Remove “%1” and everything in it?").arg(bookmark.title))
                    != QMessageBox::Yes) {
                return;
            }
            bookmarks->removeBookmark(id);
            return;
        }
        Bookmark updated = bookmark;
        updated.title = dialog.title();
        if (!folder)
            updated.url = dialog.url();
        updated.folderId = dialog.folderId();
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
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(m_tree, 1);

    auto *buttons = new QHBoxLayout;
    m_openButton = new QPushButton(Icons::icon(Icons::Open), tr("Open"));
    m_editButton = new QPushButton(tr("Edit…"));
    m_removeButton = new QPushButton(Icons::icon(Icons::Trash), tr("Delete"));
    auto *newFolder = new QPushButton(Icons::icon(Icons::Folder), tr("New Folder…"));
    auto *import = new QPushButton(tr("Import…"));
    buttons->addWidget(m_openButton);
    buttons->addWidget(m_editButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch(1);
    buttons->addWidget(newFolder);
    buttons->addWidget(import);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    const auto open = [this] {
        for (QTreeWidgetItem *item : m_tree->selectedItems()) {
            const QUrl url = item->data(1, Qt::UserRole).toUrl();
            if (!url.isEmpty())
                emit openRequested(url);
        }
    };
    connect(newFolder, &QPushButton::clicked, this, [this] {
        const QString name = QInputDialog::getText(this, tr("New Folder"), tr("Name"));
        if (name.trimmed().isEmpty())
            return;
        // Inside the selected folder, if one is selected.
        QString parent;
        for (const Bookmark &bookmark : m_bookmarks->bookmarks()) {
            if (bookmark.id == selectedId())
                parent = BookmarkManager::isFolder(bookmark) ? bookmark.id : bookmark.folderId;
        }
        m_bookmarks->createFolder(name.trimmed(), parent);
    });
    connect(import, &QPushButton::clicked, this, &BookmarksDialog::showImportMenu);
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
    const auto addItem = [this, &previous](QTreeWidgetItem *parent, const Bookmark &bookmark) {
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        const bool folder = BookmarkManager::isFolder(bookmark);
        item->setIcon(0, Icons::icon(folder ? Icons::Folder : Icons::Star));
        item->setText(0, bookmark.title.isEmpty() ? bookmark.url.host() : bookmark.title);
        if (!folder)
            item->setText(1, bookmark.url.toDisplayString());
        item->setData(0, Qt::UserRole, bookmark.id);
        item->setData(1, Qt::UserRole, bookmark.url);
        if (bookmark.id == previous)
            m_tree->setCurrentItem(item);
        return item;
    };

    if (search.isEmpty()) {
        // The folders as they are.
        const std::function<void(QTreeWidgetItem *, const QString &)> fill =
            [&](QTreeWidgetItem *parent, const QString &folderId) {
                for (const Bookmark &bookmark : m_bookmarks->children(folderId)) {
                    QTreeWidgetItem *item = addItem(parent, bookmark);
                    if (BookmarkManager::isFolder(bookmark))
                        fill(item, bookmark.id);
                }
            };
        fill(nullptr, QString());
    } else {
        // Matches, wherever they are.
        for (const Bookmark &bookmark : m_bookmarks->bookmarks()) {
            if (BookmarkManager::isFolder(bookmark))
                continue;
            if (bookmark.title.contains(search, Qt::CaseInsensitive)
                || bookmark.url.toString().contains(search, Qt::CaseInsensitive)) {
                addItem(nullptr, bookmark);
            }
        }
    }

    if (m_tree->topLevelItemCount() == 0) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, search.isEmpty() ? tr("No bookmarks yet — press Ctrl+D on a page, or import them.")
                                          : tr("No bookmarks match “%1”.").arg(search));
        item->setFlags(Qt::NoItemFlags);
        item->setFirstColumnSpanned(true);
    }
    updateButtons();
}

void BookmarksDialog::showImportMenu()
{
    QMenu menu(this);
    const auto finish = [this](const QString &from, const QList<Importer::ImportedBookmark> &found) {
        const int added = m_bookmarks->importBookmarks(found, tr("From %1").arg(from));
        QMessageBox::information(this, tr("Import bookmarks"),
                                 found.isEmpty() ? tr("No bookmarks were found.")
                                 : added == 0   ? tr("All %n bookmark(s) were already here.", nullptr, int(found.size()))
                                                : tr("Imported %n bookmark(s) into the folder “From %1”.", nullptr, added)
                                                      .arg(from));
    };
    for (const Importer::Browser &browser : Importer::installedBrowsers()) {
        connect(menu.addAction(tr("From %1").arg(browser.name)), &QAction::triggered, this,
                [browser, finish] {
                    QFile file(browser.bookmarksFile);
                    finish(browser.name, file.open(QIODevice::ReadOnly)
                                             ? Importer::parseChromiumBookmarks(file.readAll())
                                             : QList<Importer::ImportedBookmark>());
                });
    }
    if (!menu.isEmpty())
        menu.addSeparator();
    connect(menu.addAction(tr("From a bookmarks file (Firefox, Safari, any browser)…")), &QAction::triggered, this,
            [this, finish] {
                const QString path = QFileDialog::getOpenFileName(
                    this, tr("Import bookmarks"), QDir::homePath(),
                    tr("Bookmarks files (*.html *.htm *.json);;All files (*)"));
                if (path.isEmpty())
                    return;
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly))
                    return;
                const QByteArray data = file.readAll();
                const QString name = QFileInfo(path).completeBaseName();
                finish(name, data.trimmed().startsWith('{') ? Importer::parseChromiumBookmarks(data)
                                                            : Importer::parseBookmarksHtml(QString::fromUtf8(data)));
            });
    QWidget *button = qobject_cast<QWidget *>(sender());
    menu.exec(button ? button->mapToGlobal(QPoint(0, button->height())) : QCursor::pos());
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

#include "historydialog.h"

#include "historymanager.h"
#include "icons.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace spacepenguin {
namespace {

constexpr int kIdRole = Qt::UserRole;
constexpr int kUrlRole = Qt::UserRole + 1;
constexpr int kTimeRole = Qt::UserRole + 2;

/** Sorts the "Last visited" and "Visits" columns by value rather than by text. */
class HistoryItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem &other) const override
    {
        const int column = treeWidget() ? treeWidget()->sortColumn() : 0;
        if (column == 2)
            return data(2, kTimeRole).toLongLong() < other.data(2, kTimeRole).toLongLong();
        if (column == 3)
            return text(3).toInt() < other.text(3).toInt();
        return QTreeWidgetItem::operator<(other);
    }
};

} // namespace

HistoryDialog::HistoryDialog(HistoryManager *history, QWidget *parent)
    : QDialog(parent)
    , m_history(history)
{
    setWindowTitle(tr("History"));
    resize(820, 560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(10);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search history"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(Icons::icon(Icons::Search), QLineEdit::LeadingPosition);
    layout->addWidget(m_search);

    m_tree = new QTreeWidget;
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({tr("Title"), tr("Address"), tr("Last visited"), tr("Visits")});
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setSortingEnabled(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(m_tree, 1);

    auto *buttons = new QHBoxLayout;
    m_openButton = new QPushButton(Icons::icon(Icons::Open), tr("Open"));
    m_removeButton = new QPushButton(Icons::icon(Icons::Trash), tr("Delete"));
    m_clearButton = new QPushButton(tr("Clear all history…"));
    m_clearButton->setProperty("danger", true);
    buttons->addWidget(m_openButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch(1);
    buttons->addWidget(m_clearButton);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    connect(m_search, &QLineEdit::textChanged, this, &HistoryDialog::reload);
    connect(m_tree, &QTreeWidget::itemActivated, this, &HistoryDialog::openSelected);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &HistoryDialog::updateButtons);
    connect(m_openButton, &QPushButton::clicked, this, &HistoryDialog::openSelected);
    connect(m_removeButton, &QPushButton::clicked, this, &HistoryDialog::removeSelected);
    connect(m_clearButton, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Clear history"),
                                  tr("Delete every page in your browsing history? This cannot "
                                     "be undone."),
                                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
            == QMessageBox::Yes) {
            m_history->clearHistory();
        }
    });
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::close);

    if (m_history) {
        connect(m_history, &HistoryManager::visitAdded, this, &HistoryDialog::reload);
        connect(m_history, &HistoryManager::visitRemoved, this, &HistoryDialog::reload);
        connect(m_history, &HistoryManager::historyCleared, this, &HistoryDialog::reload);
    }

    // Enter in the search field must not press a button.
    for (QPushButton *button : findChildren<QPushButton *>())
        button->setAutoDefault(false);

    reload();
    m_tree->sortItems(2, Qt::DescendingOrder);
}

void HistoryDialog::reload()
{
    m_tree->setSortingEnabled(false);
    m_tree->clear();

    if (m_history) {
        const QLocale locale;
        const QList<HistoryEntry> entries = m_history->history(-1, m_search->text().trimmed());
        for (const HistoryEntry &entry : entries) {
            auto *item = new HistoryItem(m_tree);
            item->setIcon(0, Icons::icon(Icons::Globe));
            item->setText(0, entry.title.isEmpty() ? entry.url.host() : entry.title);
            item->setText(1, entry.url.toDisplayString());
            const QDateTime visited = QDateTime::fromSecsSinceEpoch(entry.visitTime);
            item->setText(2, locale.toString(visited, QLocale::ShortFormat));
            item->setData(2, kTimeRole, entry.visitTime);
            item->setText(3, QString::number(entry.visitCount));
            item->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
            item->setData(0, kIdRole, entry.id);
            item->setData(0, kUrlRole, entry.url);
            item->setToolTip(0, entry.title);
            item->setToolTip(1, entry.url.toDisplayString());
        }
    }

    m_tree->setSortingEnabled(true);
    m_clearButton->setEnabled(m_history && m_history->uniqueUrls() > 0);
    updateButtons();
}

void HistoryDialog::openSelected()
{
    const auto items = m_tree->selectedItems();
    for (QTreeWidgetItem *item : items)
        emit openRequested(item->data(0, kUrlRole).toUrl());
}

void HistoryDialog::removeSelected()
{
    QStringList ids;
    for (QTreeWidgetItem *item : m_tree->selectedItems())
        ids.append(item->data(0, kIdRole).toString());
    for (const QString &id : ids)
        m_history->removeEntry(id);
}

void HistoryDialog::updateButtons()
{
    const bool selection = !m_tree->selectedItems().isEmpty();
    m_openButton->setEnabled(selection);
    m_removeButton->setEnabled(selection);
}

}

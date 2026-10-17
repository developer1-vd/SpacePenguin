#include "downloadsdialog.h"

#include "downloadmanager.h"
#include "icons.h"
#include "preferences.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedLayout>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace spacepenguin {

/** One download: name, status line, progress and the actions that fit its state. */
class DownloadRow : public QWidget
{
public:
    DownloadRow(DownloadManager *downloads, const QString &id, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_downloads(downloads)
        , m_id(id)
    {
        setObjectName(QStringLiteral("downloadRow"));
        setAttribute(Qt::WA_StyledBackground);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 10, 8, 10);
        layout->setSpacing(10);

        auto *icon = new QLabel;
        icon->setPixmap(Icons::icon(Icons::Download).pixmap(22, 22));
        layout->addWidget(icon, 0, Qt::AlignTop);

        auto *text = new QVBoxLayout;
        text->setSpacing(4);
        m_name = new QLabel;
        m_name->setObjectName(QStringLiteral("downloadName"));
        m_status = new QLabel;
        m_status->setProperty("muted", true);
        m_progress = new QProgressBar;
        m_progress->setTextVisible(false);
        m_progress->setFixedHeight(6);
        text->addWidget(m_name);
        text->addWidget(m_progress);
        text->addWidget(m_status);
        layout->addLayout(text, 1);

        m_cancel = actionButton(Icons::Close, tr("Cancel"));
        m_open = actionButton(Icons::Open, tr("Open file"));
        m_folder = actionButton(Icons::Folder, tr("Show in folder"));
        m_remove = actionButton(Icons::Trash, tr("Remove from list"));
        layout->addWidget(m_cancel);
        layout->addWidget(m_open);
        layout->addWidget(m_folder);
        layout->addWidget(m_remove);

        connect(m_cancel, &QToolButton::clicked, this,
                [this] { m_downloads->cancelDownload(m_id); });
        connect(m_open, &QToolButton::clicked, this,
                [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_path)); });
        connect(m_folder, &QToolButton::clicked, this, [this] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_path).absolutePath()));
        });
        connect(m_remove, &QToolButton::clicked, this,
                [this] { m_downloads->removeDownload(m_id); });
    }

    void update(const DownloadItem &item)
    {
        const QLocale locale;
        m_path = item.filePath;
        m_name->setText(item.suggestedFileName.isEmpty() ? QFileInfo(item.filePath).fileName()
                                                         : item.suggestedFileName);
        m_name->setToolTip(item.url);

        const bool running = item.state == DownloadItem::InProgress
            || item.state == DownloadItem::Requested;
        const bool done = item.state == DownloadItem::Completed;

        QString status;
        switch (item.state) {
        case DownloadItem::Requested:
        case DownloadItem::InProgress:
            status = item.totalBytes > 0
                ? tr("%1 of %2").arg(locale.formattedDataSize(item.receivedBytes),
                                     locale.formattedDataSize(item.totalBytes))
                : locale.formattedDataSize(item.receivedBytes);
            break;
        case DownloadItem::Completed:
            status = tr("%1 · Done").arg(locale.formattedDataSize(item.receivedBytes));
            break;
        case DownloadItem::Cancelled:
            status = tr("Cancelled");
            break;
        case DownloadItem::Interrupted:
            status = item.interruptReason.isEmpty()
                ? tr("Failed")
                : tr("Failed — %1").arg(item.interruptReason);
            break;
        }
        m_status->setText(status);

        m_progress->setVisible(running);
        if (item.totalBytes > 0) {
            m_progress->setRange(0, 1000);
            m_progress->setValue(int(item.receivedBytes * 1000 / item.totalBytes));
        } else {
            m_progress->setRange(0, 0); // unknown size: busy indicator
        }

        m_cancel->setVisible(running);
        m_open->setVisible(done);
        m_folder->setVisible(done);
        m_remove->setVisible(!running);
    }

private:
    QToolButton *actionButton(Icons::Glyph glyph, const QString &tip)
    {
        auto *button = new QToolButton;
        button->setIcon(Icons::icon(glyph));
        button->setIconSize(QSize(18, 18));
        button->setAutoRaise(true);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        return button;
    }

    DownloadManager *m_downloads = nullptr;
    QString m_id;
    QString m_path;
    QLabel *m_name = nullptr;
    QLabel *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    QToolButton *m_cancel = nullptr;
    QToolButton *m_open = nullptr;
    QToolButton *m_folder = nullptr;
    QToolButton *m_remove = nullptr;
};

DownloadsDialog::DownloadsDialog(DownloadManager *downloads, QWidget *parent)
    : QDialog(parent)
    , m_downloads(downloads)
{
    setWindowTitle(tr("Downloads"));
    resize(560, 460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(10);

    m_list = new QListWidget;
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    m_empty = new QLabel(tr("Files you download appear here."));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setProperty("muted", true);

    auto *stackHost = new QWidget;
    m_stack = new QStackedLayout(stackHost);
    m_stack->addWidget(m_list);
    m_stack->addWidget(m_empty);
    layout->addWidget(stackHost, 1);

    auto *buttons = new QHBoxLayout;
    auto *openFolder = new QPushButton(Icons::icon(Icons::Folder), tr("Open downloads folder"));
    connect(openFolder, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Preferences::instance()->downloadDirectory()));
    });
    m_clearButton = new QPushButton(tr("Clear finished"));
    connect(m_clearButton, &QPushButton::clicked, this, [this] { m_downloads->clearFinished(); });
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::close);
    buttons->addWidget(openFolder);
    buttons->addWidget(m_clearButton);
    buttons->addStretch(1);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    if (m_downloads) {
        connect(m_downloads, &DownloadManager::downloadStarted, this,
                [this](const DownloadItem &item) { addRow(item, true); updateEmptyState(); });
        connect(m_downloads, &DownloadManager::downloadProgress, this, &DownloadsDialog::updateRow);
        connect(m_downloads, &DownloadManager::downloadFinished, this, [this](const DownloadItem &item) {
            updateRow(item);
            updateEmptyState();
        });
        connect(m_downloads, &DownloadManager::downloadRemoved, this, &DownloadsDialog::removeRow);
    }

    // Enter in the search field must not press a button.
    for (QPushButton *button : findChildren<QPushButton *>())
        button->setAutoDefault(false);

    reload();
}

void DownloadsDialog::reload()
{
    m_list->clear();
    m_items.clear();
    if (m_downloads) {
        for (const DownloadItem &item : m_downloads->downloads())
            addRow(item, false);
    }
    updateEmptyState();
}

void DownloadsDialog::addRow(const DownloadItem &item, bool atTop)
{
    auto *row = new DownloadRow(m_downloads, item.id);
    row->update(item);
    auto *listItem = new QListWidgetItem;
    listItem->setSizeHint(row->sizeHint());
    if (atTop)
        m_list->insertItem(0, listItem);
    else
        m_list->addItem(listItem);
    m_list->setItemWidget(listItem, row);
    m_items.insert(item.id, listItem);
}

void DownloadsDialog::updateRow(const DownloadItem &item)
{
    QListWidgetItem *listItem = m_items.value(item.id);
    if (!listItem)
        return;
    if (auto *row = static_cast<DownloadRow *>(m_list->itemWidget(listItem)))
        row->update(item);
}

void DownloadsDialog::removeRow(const QString &id)
{
    if (QListWidgetItem *listItem = m_items.take(id))
        delete m_list->takeItem(m_list->row(listItem));
    updateEmptyState();
}

void DownloadsDialog::updateEmptyState()
{
    m_stack->setCurrentWidget(m_items.isEmpty() ? static_cast<QWidget *>(m_empty)
                                                : static_cast<QWidget *>(m_list));
    bool anyFinished = false;
    if (m_downloads) {
        for (const DownloadItem &item : m_downloads->downloads()) {
            if (item.state != DownloadItem::InProgress && item.state != DownloadItem::Requested)
                anyFinished = true;
        }
    }
    m_clearButton->setEnabled(anyFinished);
}

}

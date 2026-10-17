#pragma once

#include <QDialog>
#include <QHash>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedLayout;

namespace spacepenguin {

class DownloadManager;
class DownloadRow;
struct DownloadItem;

/** Live list of this session's downloads with progress, cancel and open. */
class DownloadsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadsDialog(DownloadManager *downloads, QWidget *parent = nullptr);

private:
    void reload();
    void addRow(const DownloadItem &item, bool atTop);
    void updateRow(const DownloadItem &item);
    void removeRow(const QString &id);
    void updateEmptyState();

    DownloadManager *m_downloads = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_empty = nullptr;
    QStackedLayout *m_stack = nullptr;
    QPushButton *m_clearButton = nullptr;
    QHash<QString, QListWidgetItem *> m_items;
};

}

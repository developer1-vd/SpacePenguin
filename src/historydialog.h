#pragma once

#include <QDialog>
#include <QUrl>

class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace spacepenguin {

class HistoryManager;

/** Searchable list of visited pages with delete and clear. */
class HistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryDialog(HistoryManager *history, QWidget *parent = nullptr);

signals:
    void openRequested(const QUrl &url);

private:
    void reload();
    void openSelected();
    void removeSelected();
    void updateButtons();

    HistoryManager *m_history = nullptr;
    QLineEdit *m_search = nullptr;
    QTreeWidget *m_tree = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_clearButton = nullptr;
};

}

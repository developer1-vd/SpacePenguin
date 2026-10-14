#pragma once

#include <QDialog>
#include <QUrl>

class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace spacepenguin {

class BookmarkManager;

/** Name and address editor for one bookmark, with a Remove button. */
class BookmarkEditDialog : public QDialog
{
    Q_OBJECT

public:
    BookmarkEditDialog(const QString &title, const QUrl &url, QWidget *parent = nullptr);

    QString title() const;
    QUrl url() const;
    bool removeRequested() const { return m_remove; }

    /** Shows the editor for an existing bookmark and applies the result. */
    static void editBookmark(BookmarkManager *bookmarks, const QString &id, QWidget *parent);

private:
    QLineEdit *m_title = nullptr;
    QLineEdit *m_url = nullptr;
    bool m_remove = false;
};

/** Searchable list of every bookmark. */
class BookmarksDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BookmarksDialog(BookmarkManager *bookmarks, QWidget *parent = nullptr);

signals:
    void openRequested(const QUrl &url);

private:
    void reload();
    QString selectedId() const;
    void updateButtons();

    BookmarkManager *m_bookmarks = nullptr;
    QLineEdit *m_search = nullptr;
    QTreeWidget *m_tree = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_removeButton = nullptr;
};

}

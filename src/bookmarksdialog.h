#pragma once

#include <QDialog>
#include <QUrl>

class QComboBox;
class QFormLayout;
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
    /** Lets the folder be chosen: {id, path} pairs, the top level being an empty id. */
    void setFolders(const QList<QPair<QString, QString>> &folders, const QString &current);
    QString folderId() const;
    /** Edits a folder: a name and where it sits, no address. */
    void setFolderMode();

    /** Shows the editor for an existing bookmark and applies the result. */
    static void editBookmark(BookmarkManager *bookmarks, const QString &id, QWidget *parent);

private:
    QLineEdit *m_title = nullptr;
    QLineEdit *m_url = nullptr;
    QComboBox *m_folder = nullptr;
    QFormLayout *m_form = nullptr;
    QPushButton *m_save = nullptr;
    bool m_remove = false;
};

/** Every bookmark in its folders, searchable, with folders and importing. */
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
    void showImportMenu();

    BookmarkManager *m_bookmarks = nullptr;
    QLineEdit *m_search = nullptr;
    QTreeWidget *m_tree = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_removeButton = nullptr;
};

}

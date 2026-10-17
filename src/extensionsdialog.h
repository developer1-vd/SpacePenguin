#pragma once

#include <QDialog>

class QLabel;
class QListWidget;
class QPushButton;

namespace spacepenguin {

class ChromeExtensions;
class UserScripts;

class ExtensionsDialog : public QDialog
{
    Q_OBJECT

public:
    ExtensionsDialog(UserScripts *scripts, ChromeExtensions *chrome, QWidget *parent = nullptr);

signals:
    /** Open the Chrome Web Store in a tab. */
    void webStoreRequested();

private:
    void reload();
    void addScript();
    void removeSelected();
    void updateButtons();
    QString selectedId() const;

    void reloadChrome();
    QString selectedChromeId() const;

    UserScripts *m_scripts = nullptr;
    ChromeExtensions *m_chrome = nullptr;
    QListWidget *m_chromeList = nullptr;
    QPushButton *m_chromeToggle = nullptr;
    QPushButton *m_chromeRemove = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_summary = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_toggleButton = nullptr;
};

}
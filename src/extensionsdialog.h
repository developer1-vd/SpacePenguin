#pragma once

#include <QDialog>

class QLabel;
class QListWidget;
class QPushButton;

namespace spacepenguin {

class UserScripts;

class ExtensionsDialog : public QDialog
{
    Q_OBJECT

public:
    ExtensionsDialog(UserScripts *scripts, QWidget *parent = nullptr);

private:
    void reload();
    void addScript();
    void removeSelected();
    void updateButtons();
    QString selectedId() const;

    UserScripts *m_scripts = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_summary = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_toggleButton = nullptr;
};

}
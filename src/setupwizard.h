#pragma once

#include <QByteArray>
#include <QDialog>
#include <QList>

class QLabel;
class QPushButton;
class QStackedWidget;

namespace spacepenguin {

class BookmarkManager;

namespace FirstRun {

/** Whether the setup wizard opens: on a new install's first start only (no
 *  settings stored yet), in a normal window, with a screen to show it on.
 *  SPACEPENGUIN_NO_SETUP=1 keeps it away (automated runs). */
inline bool shouldShowSetup(bool settingsWereEmpty, bool privateOrTor, bool appWindow,
                            const QByteArray &platform, bool disabled)
{
    if (!settingsWereEmpty || privateOrTor || appWindow || disabled)
        return false;
    return platform != "offscreen" && platform != "minimal";
}

}

/**
 * The first start of a new install: a few questions instead of a trip through
 * Settings. The search engine, the look, privacy, what SpacePenguin is for
 * (roles) and bookmarks from another browser; each answer is stored as it is
 * given, and every one of them can be changed later in Settings.
 */
class SetupWizard : public QDialog
{
    Q_OBJECT

public:
    explicit SetupWizard(BookmarkManager *bookmarks, QWidget *parent = nullptr);

private:
    QWidget *welcomePage();
    QWidget *searchPage();
    QWidget *lookPage();
    QWidget *privacyPage();
    QWidget *rolesPage();
    QWidget *bookmarksPage();
    QWidget *donePage();
    void showPage(int index);

    BookmarkManager *m_bookmarks = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_steps = nullptr;
    QPushButton *m_back = nullptr;
    QPushButton *m_next = nullptr;
    QPushButton *m_skip = nullptr;
};

}

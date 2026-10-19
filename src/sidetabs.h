#pragma once

#include <QPoint>
#include <QWidget>

class QAction;
class QBoxLayout;
class QLabel;
class QListWidget;
class QToolButton;

namespace spacepenguin {

class TabBar;
class TabGroups;

/**
 * The tabs in a panel down the side of the window, as in Edge and Firefox.
 *
 * It is a view of the window's tab bar, which stays the one list of tabs (hidden
 * while this panel is shown): sync() copies the tabs and their groups in, and
 * choosing, moving or closing a tab here acts on the tab bar. A group shows as a
 * header row that collapses it. Collapsed, the panel narrows to the tabs' icons.
 */
class SideTabs : public QWidget
{
    Q_OBJECT

public:
    enum Role {
        KindRole = Qt::UserRole, // a Kind
        TabRole,                 // the tab's object, as quintptr
        GroupRole,               // the group id, 0 for none
        CountRole,               // on a group's header: how many tabs it has
    };
    enum Kind { TabRow, GroupRow };

    SideTabs(TabBar *tabs, TabGroups *groups, QAction *newTabAction, bool isPrivate,
             QWidget *parent = nullptr);

    /** Copies the tab bar's tabs and groups into the panel. Does nothing while hidden. */
    void sync();

    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }

    /** The close button's place in a tab's row, for drawing and for clicks. */
    QRect closeButtonRect(const QRect &row) const;
    /** Where the mouse is over the list, to light up the close button under it. */
    QPoint hoverPosition() const { return m_hover; }

signals:
    void closeRequested(int index);
    void contextMenuRequested(int index, const QPoint &globalPosition);
    void groupToggleRequested(int group);
    void groupMenuRequested(int group, const QPoint &globalPosition);
    /** The collapse button was clicked; @p collapsed is the state asked for. */
    void collapseRequested(bool collapsed);
    /** The "tabs on top" button was clicked. */
    void topTabsRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    int tabIndexOfRow(int row) const;
    void applyDrop(QObject *moved);
    void updateLayout();

    TabBar *m_tabs;
    TabGroups *m_groups;
    QListWidget *m_list = nullptr;
    QToolButton *m_newTabButton = nullptr;
    QToolButton *m_topTabsButton = nullptr;
    QToolButton *m_collapseButton = nullptr;
    QLabel *m_privateIcon = nullptr;
    QLabel *m_privateLabel = nullptr;
    QBoxLayout *m_footer = nullptr;
    bool m_collapsed = false;
    bool m_syncing = false;
    QPoint m_hover{-1, -1};
};

}

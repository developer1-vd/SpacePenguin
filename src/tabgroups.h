#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QTabBar;

namespace spacepenguin {

struct TabGroup {
    int id = 0;
    QString title;
    int color = 0; // an index into TabGroups::colorNames()
    bool collapsed = false;
};

/**
 * A window's tab groups, as in Chrome, Edge and Firefox: named, coloured sets of
 * neighbouring tabs that can be collapsed out of the way.
 *
 * Tabs are known by the object in their tab bar's tab data (the page's view). A
 * group lives as long as it has tabs; arrange() keeps each group's tabs together.
 */
class TabGroups : public QObject
{
    Q_OBJECT

public:
    explicit TabGroups(QObject *parent = nullptr);

    static QStringList colorNames();
    static QColor color(int index, bool dark);
    /** Text that reads on color(index, dark). */
    static QColor textOn(int index, bool dark);

    /** A new, empty group; @p color -1 picks the first colour not in use. */
    int create(const QString &title = QString(), int color = -1);
    const TabGroup *group(int id) const;
    QList<TabGroup> groups() const { return m_groups; }

    void setTitle(int id, const QString &title);
    void setColor(int id, int color);
    void setCollapsed(int id, bool collapsed);
    bool isCollapsed(int id) const;

    /** 0 when the tab is in no group. */
    int groupOf(const QObject *tab) const { return m_members.value(tab, 0); }
    /** Puts a tab in a group, or in none with 0. Groups left empty are removed. */
    void setGroupOf(const QObject *tab, int id);
    /** Takes every tab out of a group, which removes it. */
    void ungroup(int id);
    /** A tab has closed. */
    void forget(const QObject *tab);

    /**
     * Moves tabs so each group's tabs sit together, where the group's first tab is.
     * The tab bar emits tabMoved for each move; isArranging() is true meanwhile.
     */
    void arrange(QTabBar *tabs);
    bool isArranging() const { return m_arranging; }

signals:
    void changed();

private:
    void removeEmptyGroups();

    QList<TabGroup> m_groups;
    QHash<const QObject *, int> m_members;
    int m_nextId = 1;
    bool m_arranging = false;
};

/** The object a tab bar's tab stands for: its tab data. */
QObject *tabObject(const QTabBar *tabs, int index);

}

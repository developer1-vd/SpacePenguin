#pragma once

#include <QHash>
#include <QIcon>
#include <QPointer>
#include <QTabBar>

#include <functional>

class QToolButton;

namespace spacepenguin {

class TabGroups;

/**
 * Tab bar whose tabs shrink to share the available width, like a modern browser.
 *
 * It also draws tab groups: a coloured chip with the group's name before its first
 * tab and a line in the group's colour under its tabs. A collapsed group shows
 * only its chip.
 */
class TabBar : public QTabBar
{
    Q_OBJECT

public:
    /** What a tab shows. Kept even while the tab's group is collapsed. */
    struct Label {
        QString text;
        QIcon icon;
        QString toolTip;
    };

    explicit TabBar(QWidget *parent = nullptr);

    void setGroups(TabGroups *groups);

    void setLabel(int index, const Label &label);
    Label label(int index) const;

    /** Brings the chips, the hidden tabs of collapsed groups and the group lines up to date. */
    void refreshGroups();

    /** Which tabs' icons are buttons (a speaker, while the tab plays sound or is muted). */
    void setIconClickable(std::function<bool(int index)> clickable) { m_iconClickable = std::move(clickable); }

    /** Takes a group's chip off a tab about to be removed, which would delete it. */
    void releaseChip(int index);

signals:
    /** A tab was added or removed. */
    void tabsChanged();
    /** The user dragged @p tab (its tab data) to a new place. */
    void dragFinished(QObject *tab);
    void groupChipClicked(int group);
    void groupMenuRequested(int group, const QPoint &globalPosition);
    /** The tab's icon was clicked (it shows a speaker while the tab plays sound). */
    void iconClicked(int index);

protected:
    QSize tabSizeHint(int index) const override;
    QSize minimumTabSizeHint(int index) const override;
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int groupAt(int index) const;
    bool isGroupStart(int index) const;
    /** The first tab of a collapsed group, which shows only the group's chip. */
    bool isChipOnly(int index) const;
    int chipWidth(int index) const;
    QRect iconRect(int index) const;
    QToolButton *chip(int group);

    TabGroups *m_groups = nullptr;
    QHash<const QObject *, Label> m_labels;
    QHash<int, QPointer<QToolButton>> m_chips;
    QPointer<QObject> m_pressed;
    QPointer<QObject> m_dragged;
    std::function<bool(int)> m_iconClickable;
};

}

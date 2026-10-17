#include "tabbar.h"

#include "tabgroups.h"
#include "theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QSet>
#include <QToolButton>

namespace spacepenguin {
namespace {

constexpr int kMinimumTabWidth = 72;
constexpr int kMaximumTabWidth = 240;
// Room a collapsed group's tab gives its chip on either side.
constexpr int kChipPadding = 8;

} // namespace

TabBar::TabBar(QWidget *parent)
    : QTabBar(parent)
{
    setObjectName(QStringLiteral("tabBar"));
    setExpanding(false);
    setMovable(true);
    setTabsClosable(false); // each tab gets its own themed close button
    setUsesScrollButtons(true);
    setElideMode(Qt::ElideRight);
    setDocumentMode(true);
    setDrawBase(false);
    setIconSize(QSize(16, 16));
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    connect(this, &QTabBar::tabMoved, this, [this] {
        // Only the user's drags; moves made to keep groups together are not news.
        if (m_pressed)
            m_dragged = m_pressed;
    });
}

void TabBar::setGroups(TabGroups *groups)
{
    m_groups = groups;
}

void TabBar::setLabel(int index, const Label &label)
{
    QObject *tab = tabObject(this, index);
    if (!tab)
        return;
    m_labels.insert(tab, label);
    if (!isChipOnly(index)) {
        setTabText(index, label.text);
        setTabIcon(index, label.icon);
    }
    setTabToolTip(index, label.toolTip);
}

TabBar::Label TabBar::label(int index) const
{
    const auto found = m_labels.constFind(tabObject(this, index));
    if (found != m_labels.constEnd())
        return found.value();
    return Label{tabText(index), tabIcon(index), tabToolTip(index)};
}

int TabBar::groupAt(int index) const
{
    return m_groups && index >= 0 && index < count() ? m_groups->groupOf(tabObject(this, index))
                                                       : 0;
}

bool TabBar::isGroupStart(int index) const
{
    const int group = groupAt(index);
    return group && (index == 0 || groupAt(index - 1) != group);
}

bool TabBar::isChipOnly(int index) const
{
    return isGroupStart(index) && m_groups->isCollapsed(groupAt(index));
}

int TabBar::chipWidth(int index) const
{
    QWidget *button = tabButton(index, QTabBar::LeftSide);
    return button ? button->sizeHint().width() : 0;
}

QToolButton *TabBar::chip(int group)
{
    QPointer<QToolButton> &button = m_chips[group];
    if (!button) {
        button = new QToolButton(this);
        button->setObjectName(QStringLiteral("groupChip"));
        button->setFocusPolicy(Qt::NoFocus);
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QToolButton::clicked, this, [this, group] { emit groupChipClicked(group); });
        QToolButton *self = button;
        connect(button, &QToolButton::customContextMenuRequested, this,
                [this, group, self](const QPoint &position) {
                    emit groupMenuRequested(group, self->mapToGlobal(position));
                });
    }
    return button;
}

void TabBar::releaseChip(int index)
{
    if (tabButton(index, QTabBar::LeftSide))
        setTabButton(index, QTabBar::LeftSide, nullptr);
}

void TabBar::refreshGroups()
{
    const bool dark = Theme::isDark();

    // Take every chip off the tabs it no longer belongs to before handing any out,
    // so no chip is ever on two tabs.
    for (int i = 0; i < count(); ++i) {
        QWidget *current = tabButton(i, QTabBar::LeftSide);
        const int group = isGroupStart(i) ? groupAt(i) : 0;
        if (current && (!group || current != m_chips.value(group)))
            setTabButton(i, QTabBar::LeftSide, nullptr);
    }

    QSet<int> shown;
    QSet<const QObject *> present;
    for (int i = 0; i < count(); ++i) {
        present.insert(tabObject(this, i));
        const int group = groupAt(i);
        const TabGroup *info = group ? m_groups->group(group) : nullptr;
        const bool start = isGroupStart(i);
        const bool collapsed = info && info->collapsed;
        setTabVisible(i, !collapsed || start);
        // A collapsed group's chip has no close button.
        if (QWidget *close = tabButton(i, QTabBar::RightSide))
            close->setVisible(!collapsed);

        if (start && info) {
            QToolButton *button = chip(group);
            const QColor fill = TabGroups::color(info->color, dark);
            const QColor text = TabGroups::textOn(info->color, dark);
            button->setText(info->title);
            button->setToolTip(collapsed ? tr("%1 — click to show its tabs").arg(info->title)
                                         : tr("%1 — click to collapse").arg(info->title));
            // An unnamed group is a dot, as in other browsers.
            const QString padding = info->title.isEmpty() ? QStringLiteral("0") : QStringLiteral("1px 9px");
            button->setStyleSheet(QStringLiteral("QToolButton { background: %1; color: %2; border: none;"
                                                 " border-radius: 9px; padding: %3; margin: 0;"
                                                 " font-weight: 600; min-width: %4px; min-height: 18px; }")
                                      .arg(fill.name(), text.name(), padding,
                                           info->title.isEmpty() ? QStringLiteral("18") : QStringLiteral("0")));
            // The tab bar lays a button out at its current size, so fit it to its text.
            button->adjustSize();
            if (tabButton(i, QTabBar::LeftSide) != button)
                setTabButton(i, QTabBar::LeftSide, button);
            button->show();
            shown.insert(group);
        }

        const Label tab = label(i);
        if (start && collapsed) {
            setTabText(i, QString());
            setTabIcon(i, QIcon());
        } else {
            if (tabText(i) != tab.text)
                setTabText(i, tab.text);
            setTabIcon(i, tab.icon);
        }
    }

    for (auto it = m_chips.begin(); it != m_chips.end();) {
        if (!shown.contains(it.key())) {
            if (it.value())
                it.value()->deleteLater();
            it = m_chips.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_labels.begin(); it != m_labels.end();) {
        if (!present.contains(it.key()))
            it = m_labels.erase(it);
        else
            ++it;
    }

    // Tab sizes depend on the groups; have them measured again.
    setElideMode(elideMode());
    update();
}

QSize TabBar::tabSizeHint(int index) const
{
    QSize hint = QTabBar::tabSizeHint(index);
    if (isChipOnly(index)) {
        hint.setWidth(chipWidth(index) + 2 * kChipPadding);
        return hint;
    }
    // Tabs share what the collapsed groups leave.
    int reserved = 0;
    int tabs = 0;
    for (int i = 0; i < count(); ++i) {
        if (!isTabVisible(i))
            continue;
        if (isChipOnly(i))
            reserved += chipWidth(i) + 2 * kChipPadding;
        else
            ++tabs;
    }
    const int share = (width() - reserved) / qMax(1, tabs);
    hint.setWidth(qBound(kMinimumTabWidth, share, kMaximumTabWidth) + chipWidth(index));
    return hint;
}

QSize TabBar::minimumTabSizeHint(int index) const
{
    QSize hint = QTabBar::minimumTabSizeHint(index);
    if (isChipOnly(index))
        hint.setWidth(chipWidth(index) + 2 * kChipPadding);
    else
        hint.setWidth(kMinimumTabWidth + chipWidth(index));
    return hint;
}

void TabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    emit tabsChanged();
}

void TabBar::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);
    emit tabsChanged();
}

void TabBar::paintEvent(QPaintEvent *event)
{
    QTabBar::paintEvent(event);
    if (!m_groups)
        return;

    // A line in the group's colour under its tabs, from its chip to its last tab.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    const bool dark = Theme::isDark();
    for (int i = 0; i < count(); ++i) {
        const int group = groupAt(i);
        if (!group || !isTabVisible(i) || isChipOnly(i))
            continue;
        const TabGroup *info = m_groups->group(group);
        if (!info)
            continue;
        const QRect rect = tabRect(i);
        const bool first = isGroupStart(i);
        const bool last = groupAt(i + 1) != group;
        const qreal left = rect.left() + (first ? 8 : 0);
        const qreal right = rect.right() + 1 - (last ? 8 : 0);
        painter.setBrush(TabGroups::color(info->color, dark));
        painter.drawRoundedRect(QRectF(left, rect.bottom() - 3.5, right - left, 2.5), 1.25, 1.25);
    }
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
    // The tab being dragged; tabMoved names the tabs it passes, not it.
    if (event->button() == Qt::LeftButton)
        m_pressed = tabObject(this, tabAt(event->position().toPoint()));
    QTabBar::mousePressEvent(event);
}

void TabBar::mouseReleaseEvent(QMouseEvent *event)
{
    QTabBar::mouseReleaseEvent(event);
    m_pressed = nullptr;
    if (m_dragged) {
        QObject *tab = m_dragged;
        m_dragged = nullptr;
        emit dragFinished(tab);
    }
}

}

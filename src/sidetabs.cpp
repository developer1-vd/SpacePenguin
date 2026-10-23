#include "sidetabs.h"

#include "icons.h"
#include "tor.h"
#include "tabbar.h"
#include "tabgroups.h"
#include "theme.h"

#include <QAction>
#include <QBoxLayout>
#include <QDropEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QToolButton>

#include <functional>

namespace spacepenguin {
namespace {

constexpr int kExpandedWidth = 248;
constexpr int kCollapsedWidth = 56;
constexpr int kTabRowHeight = 36;
constexpr int kGroupRowHeight = 32;
constexpr int kRowMargin = 6;   // between a row's background and the panel's edges
constexpr int kGroupIndent = 10; // room for a group's coloured bar

/** A list that tells its owner which tab was dropped where. */
class TabList : public QListWidget
{
public:
    std::function<void(QObject *moved)> dropped;
    std::function<void()> dragEnded;
    bool dragging = false;

protected:
    void startDrag(Qt::DropActions actions) override
    {
        // Runs until the drop; the list is not rebuilt meanwhile.
        dragging = true;
        QListWidget::startDrag(actions);
        dragging = false;
        if (dragEnded)
            dragEnded();
    }

    void dropEvent(QDropEvent *event) override
    {
        QListWidgetItem *item = currentItem();
        auto *moved = item ? reinterpret_cast<QObject *>(item->data(SideTabs::TabRole).value<quintptr>())
                           : nullptr;
        QListWidget::dropEvent(event);
        if (moved && dropped)
            dropped(moved);
    }
};

class TabDelegate : public QStyledItemDelegate
{
public:
    TabDelegate(SideTabs *panel, TabGroups *groups)
        : QStyledItemDelegate(panel)
        , m_panel(panel)
        , m_groups(groups)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const bool group = index.data(SideTabs::KindRole).toInt() == SideTabs::GroupRow;
        return QSize(option.rect.width(), group ? kGroupRowHeight : kTabRowHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const Theme::Colors colors = Theme::currentColors();
        const bool narrow = m_panel->isCollapsed();
        const bool hovered = option.state & QStyle::State_MouseOver;
        const int groupId = index.data(SideTabs::GroupRole).toInt();
        const TabGroup *group = groupId ? m_groups->group(groupId) : nullptr;
        const QColor groupColor = group ? TabGroups::color(group->color, colors.dark) : QColor();

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);

        if (index.data(SideTabs::KindRole).toInt() == SideTabs::GroupRow) {
            const QRect row = option.rect.adjusted(kRowMargin, 2, -kRowMargin, -2);
            if (hovered) {
                painter->setBrush(colors.raised);
                painter->drawRoundedRect(row, 10, 10);
            }
            if (narrow) {
                // A pill in the group's colour; the tooltip names it.
                painter->setBrush(groupColor);
                painter->drawRoundedRect(QRectF(row.center().x() - 12, row.center().y() - 4, 24, 8), 4, 4);
            } else {
                painter->setBrush(groupColor);
                painter->drawEllipse(QPointF(row.left() + 13, row.center().y() + 0.5), 5, 5);
                QRect text = row.adjusted(26, 0, -28, 0);
                QFont font = option.font;
                font.setBold(true);
                painter->setFont(font);
                QString title = group && !group->title.isEmpty() ? group->title : QObject::tr("Group");
                if (group && group->collapsed)
                    title += QStringLiteral("  ·  %1").arg(index.data(SideTabs::CountRole).toInt());
                painter->setPen(group && !group->title.isEmpty() ? colors.text : colors.mutedText);
                painter->drawText(text, Qt::AlignVCenter | Qt::AlignLeft,
                                  painter->fontMetrics().elidedText(title, Qt::ElideRight, text.width()));
                const Icons::Glyph chevron = group && group->collapsed ? Icons::ChevronRight : Icons::ChevronDown;
                Icons::icon(chevron).paint(painter, QRect(row.right() - 22, row.center().y() - 8, 16, 16));
            }
            painter->restore();
            return;
        }

        QRect row = option.rect.adjusted(kRowMargin, 1, -kRowMargin, -1);
        if (group) {
            // One bar down the group's tabs, joining the rows.
            painter->setBrush(groupColor);
            painter->drawRect(QRectF(option.rect.left() + kRowMargin + 2, option.rect.top(), 3,
                                     option.rect.height()));
            row.setLeft(row.left() + kGroupIndent);
        }

        const bool selected = option.state & QStyle::State_Selected;
        if (selected) {
            painter->setBrush(colors.window);
            painter->setPen(QPen(colors.border, 1));
            painter->drawRoundedRect(QRectF(row).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
            painter->setPen(Qt::NoPen);
        } else if (hovered) {
            painter->setBrush(colors.raised);
            painter->drawRoundedRect(row, 10, 10);
        }

        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        if (narrow) {
            icon.paint(painter, QRect(row.center().x() - 8, row.center().y() - 8, 16, 16));
            painter->restore();
            return;
        }
        icon.paint(painter, QRect(row.left() + 10, row.center().y() - 8, 16, 16));

        const bool showClose = selected || hovered;
        QRect text = row.adjusted(34, 0, showClose ? -32 : -10, 0);
        painter->setFont(option.font);
        painter->setPen(selected ? colors.text : colors.mutedText);
        const QString title = index.data(Qt::DisplayRole).toString();
        painter->drawText(text, Qt::AlignVCenter | Qt::AlignLeft,
                          painter->fontMetrics().elidedText(title, Qt::ElideRight, text.width()));

        if (showClose) {
            const QRect close = m_panel->closeButtonRect(option.rect);
            if (close.contains(m_panel->hoverPosition())) {
                painter->setBrush(colors.border);
                painter->drawEllipse(close);
            }
            Icons::icon(Icons::Close).paint(painter, close.adjusted(4, 4, -4, -4));
        }
        painter->restore();
    }

private:
    SideTabs *m_panel;
    TabGroups *m_groups;
};

QToolButton *panelButton(Icons::Glyph glyph, const QString &toolTip)
{
    auto *button = new QToolButton;
    button->setIcon(Icons::icon(glyph));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(toolTip);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

SideTabs::SideTabs(TabBar *tabs, TabGroups *groups, QAction *newTabAction, bool isPrivate,
                   QWidget *parent)
    : QWidget(parent)
    , m_tabs(tabs)
    , m_groups(groups)
{
    setObjectName(QStringLiteral("sideTabs"));
    setAttribute(Qt::WA_StyledBackground);
    setProperty("private", isPrivate);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(4);

    if (isPrivate) {
        auto *badge = new QHBoxLayout;
        badge->setContentsMargins(14, 0, 8, 2);
        m_privateIcon = new QLabel;
        m_privateIcon->setPixmap(Icons::icon(Icons::Private).pixmap(18, 18));
        m_privateIcon->setToolTip(tr("Private window: nothing from it is written to disk"));
        m_privateLabel = new QLabel(Tor::isActive() ? tr("Private · Tor") : tr("Private"));
        m_privateLabel->setObjectName(QStringLiteral("privateBadge"));
        badge->addWidget(m_privateIcon);
        badge->addWidget(m_privateLabel, 1);
        layout->addLayout(badge);
    }

    m_newTabButton = new QToolButton;
    m_newTabButton->setObjectName(QStringLiteral("sideNewTab"));
    m_newTabButton->setDefaultAction(newTabAction);
    m_newTabButton->setIconSize(QSize(18, 18));
    m_newTabButton->setFocusPolicy(Qt::NoFocus);
    m_newTabButton->setAutoRaise(true);
    m_newTabButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *newTabRow = new QHBoxLayout;
    newTabRow->setContentsMargins(kRowMargin, 0, kRowMargin, 0);
    newTabRow->addWidget(m_newTabButton);
    layout->addLayout(newTabRow);

    auto *list = new TabList;
    m_list = list;
    m_list->setObjectName(QStringLiteral("sideTabList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setMouseTracking(true);
    m_list->viewport()->setAttribute(Qt::WA_Hover);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setItemDelegate(new TabDelegate(this, m_groups));
    m_list->viewport()->installEventFilter(this);
    layout->addWidget(m_list, 1);
    list->dropped = [this](QObject *moved) { applyDrop(moved); };
    list->dragEnded = [this] { sync(); };

    m_topTabsButton = panelButton(Icons::SideTabs, tr("Show tabs on top"));
    connect(m_topTabsButton, &QToolButton::clicked, this, &SideTabs::topTabsRequested);
    m_collapseButton = panelButton(Icons::ChevronLeft, QString());
    connect(m_collapseButton, &QToolButton::clicked, this,
            [this] { emit collapseRequested(!m_collapsed); });
    m_footer = new QBoxLayout(QBoxLayout::LeftToRight);
    m_footer->setContentsMargins(kRowMargin + 2, 0, kRowMargin + 2, 0);
    m_footer->setSpacing(2);
    m_footer->addWidget(m_topTabsButton);
    m_footer->addStretch(1);
    m_footer->addWidget(m_collapseButton);
    layout->addLayout(m_footer);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        const int index = tabIndexOfRow(row);
        if (!m_syncing && index >= 0 && index != m_tabs->currentIndex())
            m_tabs->setCurrentIndex(index);
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        const QPoint global = m_list->viewport()->mapToGlobal(position);
        QListWidgetItem *item = m_list->itemAt(position);
        if (item && item->data(KindRole).toInt() == GroupRow)
            emit groupMenuRequested(item->data(GroupRole).toInt(), global);
        else
            emit contextMenuRequested(item ? tabIndexOfRow(m_list->row(item)) : -1, global);
    });

    updateLayout();
}

QRect SideTabs::closeButtonRect(const QRect &row) const
{
    return QRect(row.right() - kRowMargin - 28, row.center().y() - 10, 20, 20);
}

int SideTabs::tabIndexOfRow(int row) const
{
    QListWidgetItem *item = m_list->item(row);
    if (!item || item->data(KindRole).toInt() != TabRow)
        return -1;
    auto *tab = reinterpret_cast<QObject *>(item->data(TabRole).value<quintptr>());
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (tabObject(m_tabs, i) == tab)
            return i;
    }
    return -1;
}

void SideTabs::sync()
{
    // Rebuilding during a drag would drop what is being dragged; the drop resyncs.
    if (isHidden() || static_cast<TabList *>(m_list)->dragging)
        return;

    struct Row {
        Kind kind;
        quintptr tab = 0;
        int group = 0;
        QString text;
        QIcon icon;
        QString toolTip;
        int count = 0;
        bool hidden = false;
    };
    QList<Row> rows;
    int currentRow = -1;

    QHash<int, int> counts;
    for (int i = 0; i < m_tabs->count(); ++i)
        ++counts[m_groups->groupOf(tabObject(m_tabs, i))];

    int previousGroup = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        QObject *tab = tabObject(m_tabs, i);
        const int groupId = m_groups->groupOf(tab);
        const TabGroup *group = m_groups->group(groupId);
        if (group && groupId != previousGroup) {
            rows.append({GroupRow, 0, groupId, group->title, QIcon(),
                         group->collapsed ? tr("%1 — click to show its tabs").arg(group->title)
                                          : tr("%1 — click to collapse").arg(group->title),
                         counts.value(groupId), false});
        }
        previousGroup = groupId;
        const TabBar::Label label = m_tabs->label(i);
        if (i == m_tabs->currentIndex())
            currentRow = rows.size();
        rows.append({TabRow, reinterpret_cast<quintptr>(tab), group ? groupId : 0, label.text,
                     label.icon, label.toolTip, 0, group && group->collapsed});
    }

    m_syncing = true;
    // Rows are rebuilt only when tabs or groups came, went or moved; otherwise they
    // are updated where they are, so a click or a drag is never pulled from under
    // the mouse.
    bool same = m_list->count() == rows.size();
    for (int row = 0; same && row < rows.size(); ++row) {
        const QListWidgetItem *item = m_list->item(row);
        same = item->data(KindRole).toInt() == rows.at(row).kind
            && item->data(TabRole).value<quintptr>() == rows.at(row).tab
            && item->data(GroupRole).toInt() == rows.at(row).group;
    }
    const int scroll = m_list->verticalScrollBar()->value();
    if (!same) {
        m_list->clear();
        for (const Row &row : std::as_const(rows)) {
            auto *item = new QListWidgetItem(m_list);
            item->setData(KindRole, row.kind);
            item->setData(TabRole, QVariant::fromValue(row.tab));
            item->setData(GroupRole, row.group);
            item->setFlags(row.kind == GroupRow
                               ? Qt::ItemIsEnabled
                               : Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        }
    }
    for (int index = 0; index < rows.size(); ++index) {
        const Row &row = rows.at(index);
        QListWidgetItem *item = m_list->item(index);
        if (item->text() != row.text)
            item->setText(row.text);
        if (item->icon().cacheKey() != row.icon.cacheKey())
            item->setIcon(row.icon);
        item->setToolTip(row.toolTip);
        item->setData(CountRole, row.count);
        m_list->setRowHidden(index, row.hidden);
    }

    if (m_list->currentRow() != currentRow)
        m_list->setCurrentRow(currentRow);
    if (!same)
        m_list->verticalScrollBar()->setValue(scroll);
    if (QListWidgetItem *current = m_list->currentItem())
        m_list->scrollToItem(current, QAbstractItemView::EnsureVisible);
    m_list->viewport()->update();
    m_syncing = false;
}

void SideTabs::applyDrop(QObject *moved)
{
    // The tab joins the group it was dropped into: under a group's header or after
    // one of its tabs. Anywhere else, it leaves its group.
    QList<QObject *> order;
    int newGroup = 0;
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        if (item->data(KindRole).toInt() != TabRow)
            continue;
        auto *tab = reinterpret_cast<QObject *>(item->data(TabRole).value<quintptr>());
        order.append(tab);
        if (tab != moved)
            continue;
        for (int above = row - 1; above >= 0; --above) {
            if (m_list->isRowHidden(above))
                continue;
            const int group = m_list->item(above)->data(GroupRole).toInt();
            newGroup = m_groups->isCollapsed(group) ? 0 : group;
            break;
        }
    }

    for (int target = 0; target < order.size(); ++target) {
        for (int from = target; from < m_tabs->count(); ++from) {
            if (tabObject(m_tabs, from) == order.at(target)) {
                if (from != target)
                    m_tabs->moveTab(from, target);
                break;
            }
        }
    }
    m_groups->setGroupOf(moved, newGroup);
    m_groups->arrange(m_tabs);
    m_tabs->refreshGroups();
    sync();
}

void SideTabs::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed)
        return;
    m_collapsed = collapsed;
    updateLayout();
    m_list->viewport()->update();
}

void SideTabs::updateLayout()
{
    setFixedWidth(m_collapsed ? kCollapsedWidth : kExpandedWidth);
    m_newTabButton->setToolButtonStyle(m_collapsed ? Qt::ToolButtonIconOnly
                                                   : Qt::ToolButtonTextBesideIcon);
    if (m_privateLabel)
        m_privateLabel->setVisible(!m_collapsed);
    // Narrow, the footer's buttons stack.
    m_footer->setDirection(m_collapsed ? QBoxLayout::BottomToTop : QBoxLayout::LeftToRight);
    m_collapseButton->setIcon(Icons::icon(m_collapsed ? Icons::ChevronRight : Icons::ChevronLeft));
    m_collapseButton->setToolTip(m_collapsed ? tr("Expand the tab panel") : tr("Collapse the tab panel"));
}

bool SideTabs::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_list->viewport())
        return QWidget::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseMove:
        m_hover = static_cast<QMouseEvent *>(event)->position().toPoint();
        m_list->viewport()->update();
        break;
    case QEvent::Leave:
        m_hover = QPoint(-1, -1);
        m_list->viewport()->update();
        break;
    case QEvent::MouseButtonPress: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const QPoint position = mouse->position().toPoint();
        QListWidgetItem *item = m_list->itemAt(position);
        if (!item || item->data(KindRole).toInt() != TabRow)
            break;
        const int index = tabIndexOfRow(m_list->row(item));
        if (mouse->button() == Qt::LeftButton && !m_collapsed
            && closeButtonRect(m_list->visualItemRect(item)).contains(position)) {
            emit closeRequested(index);
            return true;
        }
        if (mouse->button() == Qt::MiddleButton) {
            emit closeRequested(index);
            return true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        QListWidgetItem *item = m_list->itemAt(mouse->position().toPoint());
        if (mouse->button() == Qt::LeftButton && item && item->data(KindRole).toInt() == GroupRow) {
            emit groupToggleRequested(item->data(GroupRole).toInt());
            return true;
        }
        break;
    }
    case QEvent::MouseButtonDblClick: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (!m_list->itemAt(mouse->position().toPoint())) {
            m_newTabButton->click();
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

}

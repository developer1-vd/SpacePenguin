#include "tabgroups.h"

#include <QSet>
#include <QTabBar>

namespace spacepenguin {
namespace {

struct GroupColor {
    const char *name;
    QRgb light;
    QRgb dark;
};

// Strong on the light theme, soft on the dark one, so a group stands out on both.
constexpr GroupColor kColors[] = {
    {QT_TRANSLATE_NOOP("TabGroups", "Grey"), 0x5f6368, 0xdadce0},
    {QT_TRANSLATE_NOOP("TabGroups", "Blue"), 0x1a73e8, 0x8ab4f8},
    {QT_TRANSLATE_NOOP("TabGroups", "Red"), 0xd93025, 0xf28b82},
    {QT_TRANSLATE_NOOP("TabGroups", "Yellow"), 0xe8a200, 0xfdd663},
    {QT_TRANSLATE_NOOP("TabGroups", "Green"), 0x1e8e3e, 0x81c995},
    {QT_TRANSLATE_NOOP("TabGroups", "Pink"), 0xd01884, 0xff8bcb},
    {QT_TRANSLATE_NOOP("TabGroups", "Purple"), 0x9334e6, 0xc58af9},
    {QT_TRANSLATE_NOOP("TabGroups", "Cyan"), 0x007b83, 0x78d9ec},
    {QT_TRANSLATE_NOOP("TabGroups", "Orange"), 0xe8710a, 0xfcad70},
};
constexpr int kColorCount = int(sizeof(kColors) / sizeof(kColors[0]));

int clampColor(int index)
{
    return index >= 0 && index < kColorCount ? index : 0;
}

} // namespace

QObject *tabObject(const QTabBar *tabs, int index)
{
    return tabs->tabData(index).value<QObject *>();
}

TabGroups::TabGroups(QObject *parent)
    : QObject(parent)
{
}

QStringList TabGroups::colorNames()
{
    QStringList names;
    for (const GroupColor &color : kColors)
        names.append(tr(color.name));
    return names;
}

QColor TabGroups::color(int index, bool dark)
{
    const GroupColor &color = kColors[clampColor(index)];
    return QColor::fromRgb(dark ? color.dark : color.light);
}

QColor TabGroups::textOn(int index, bool dark)
{
    // Dark text on the soft dark-theme colours and on light yellow and orange.
    const int i = clampColor(index);
    const bool darkText = dark || i == 3 || i == 8;
    return darkText ? QColor(0x20, 0x21, 0x24) : QColor(Qt::white);
}

int TabGroups::create(const QString &title, int color)
{
    if (color < 0) {
        QSet<int> used;
        for (const TabGroup &group : m_groups)
            used.insert(group.color);
        color = 1; // blue first, like other browsers; grey last
        for (int i = 1; i <= kColorCount; ++i) {
            if (!used.contains(i % kColorCount)) {
                color = i % kColorCount;
                break;
            }
        }
    }
    TabGroup group;
    group.id = m_nextId++;
    group.title = title;
    group.color = clampColor(color);
    m_groups.append(group);
    emit changed();
    return group.id;
}

const TabGroup *TabGroups::group(int id) const
{
    for (const TabGroup &group : m_groups) {
        if (group.id == id)
            return &group;
    }
    return nullptr;
}

void TabGroups::setTitle(int id, const QString &title)
{
    for (TabGroup &group : m_groups) {
        if (group.id == id && group.title != title) {
            group.title = title;
            emit changed();
        }
    }
}

void TabGroups::setColor(int id, int color)
{
    for (TabGroup &group : m_groups) {
        if (group.id == id && group.color != clampColor(color)) {
            group.color = clampColor(color);
            emit changed();
        }
    }
}

void TabGroups::setCollapsed(int id, bool collapsed)
{
    for (TabGroup &group : m_groups) {
        if (group.id == id && group.collapsed != collapsed) {
            group.collapsed = collapsed;
            emit changed();
        }
    }
}

bool TabGroups::isCollapsed(int id) const
{
    const TabGroup *found = group(id);
    return found && found->collapsed;
}

void TabGroups::setGroupOf(const QObject *tab, int id)
{
    if (!tab || groupOf(tab) == id)
        return;
    if (id && group(id))
        m_members.insert(tab, id);
    else
        m_members.remove(tab);
    removeEmptyGroups();
    emit changed();
}

void TabGroups::ungroup(int id)
{
    for (auto it = m_members.begin(); it != m_members.end();) {
        if (it.value() == id)
            it = m_members.erase(it);
        else
            ++it;
    }
    removeEmptyGroups();
    emit changed();
}

void TabGroups::forget(const QObject *tab)
{
    if (m_members.remove(tab)) {
        removeEmptyGroups();
        emit changed();
    }
}

void TabGroups::removeEmptyGroups()
{
    QSet<int> inUse;
    for (const int id : std::as_const(m_members))
        inUse.insert(id);
    m_groups.removeIf([&inUse](const TabGroup &group) { return !inUse.contains(group.id); });
}

void TabGroups::arrange(QTabBar *tabs)
{
    // The order wanted: tabs as they are, except that the first tab of a group
    // pulls the rest of its group in right behind it.
    QList<QObject *> order;
    QSet<int> placed;
    for (int i = 0; i < tabs->count(); ++i) {
        QObject *tab = tabObject(tabs, i);
        const int id = groupOf(tab);
        if (!id) {
            order.append(tab);
            continue;
        }
        if (placed.contains(id))
            continue;
        placed.insert(id);
        for (int j = i; j < tabs->count(); ++j) {
            if (groupOf(tabObject(tabs, j)) == id)
                order.append(tabObject(tabs, j));
        }
    }

    m_arranging = true;
    for (int target = 0; target < order.size(); ++target) {
        if (tabObject(tabs, target) == order.at(target))
            continue;
        for (int from = target + 1; from < tabs->count(); ++from) {
            if (tabObject(tabs, from) == order.at(target)) {
                tabs->moveTab(from, target);
                break;
            }
        }
    }
    m_arranging = false;
}

}

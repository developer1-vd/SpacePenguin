#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

class QSettings;

namespace spacepenguin {

/**
 * The open tabs, as kept in the settings so they come back on the next start and
 * after a crash: each tab's address and back/forward history, the tab groups, and
 * which tab was in front. BrowserWindow fills one from its tabs and builds tabs
 * from one; this is the part that touches the disk.
 */
namespace Session {

struct Group {
    QString title;
    int color = 0;
    bool collapsed = false;
};

struct Saved {
    QStringList urls;
    QList<QByteArray> histories; // per tab, QWebEngineHistory's own stream; empty for none
    QList<int> tabGroups;        // per tab, a place in groups; -1 for none
    QList<Group> groups;
    int currentTab = 0;

    bool isEmpty() const { return urls.isEmpty(); }
};

/** Where the tabs are kept, and where they are set aside after a crash. */
inline const QString kGroup = QStringLiteral("session");
inline const QString kCrashedGroup = QStringLiteral("crashedSession");

void write(QSettings &settings, const Saved &session, const QString &group = kGroup);
/** At most @p maximumTabs tabs, with currentTab within them. */
Saved read(QSettings &settings, int maximumTabs, const QString &group = kGroup);

/**
 * Marks SpacePenguin as running; returns whether the last run never got as far as
 * markClosed() (it crashed, was killed, or the power went).
 */
bool markRunning(QSettings &settings);
void markClosed(QSettings &settings);

/**
 * After a crash with restoring turned off: copies the tabs aside, as the new
 * window's own will soon be saved over them. Returns whether there were any.
 */
bool setAsideForRestore(QSettings &settings);

}

}

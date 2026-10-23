#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace spacepenguin {

/**
 * What someone uses SpacePenguin for. Each role picked in Settings › Made for you
 * brings settings and tools of its own; the browser is the same for everyone else.
 *
 *   developer  developer tools (F12), page source (Ctrl+U), no cache, own user agent
 *   gamer      games lock the mouse pointer without asking; other tabs go quiet in full screen
 *   student    joins a teacher's class (classroom.h), Focus mode, Copy citation
 *   worker     Focus mode, work pages opened together (and at start-up)
 *   teacher    runs a class (classroom.h), a larger page size for the projector
 */
namespace Roles {

enum class Role { Developer, Gamer, Student, Worker, Teacher };

QList<Role> all();
/** The name stored in the settings: "developer", "gamer", … */
QString key(Role role);
QString label(Role role);
QString description(Role role);

/** Focus mode comes with the student and worker roles. */
bool hasFocusMode(const QStringList &roles);

/** "youtube.com" from "https://www.youtube.com/watch", " YouTube.com/ " or "www.youtube.com". */
QString normalizeSite(const QString &text);
/** Whether Focus mode keeps @p url away: its host is one of @p sites, or under one. */
bool isBlocked(const QUrl &url, const QStringList &sites);
/** The page shown in place of a site that Focus mode keeps away. */
QString blockedPage(const QUrl &url);

/** A reference to a web page: Title. Site. URL (accessed 21 October 2026). */
QString citation(const QString &title, const QUrl &url, const QDate &accessed);

}

}

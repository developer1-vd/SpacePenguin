#include "session.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace spacepenguin::Session {
namespace {

const QString kRunning = QStringLiteral("session/running");

}

void write(QSettings &settings, const Saved &session, const QString &group)
{
    QStringList histories;
    for (const QByteArray &history : session.histories)
        histories.append(QString::fromLatin1(history.toBase64()));
    QJsonArray groupList;
    for (const Group &entry : session.groups) {
        groupList.append(QJsonObject{{QStringLiteral("title"), entry.title},
                                     {QStringLiteral("color"), entry.color},
                                     {QStringLiteral("collapsed"), entry.collapsed}});
    }
    QJsonArray tabGroups;
    for (int place : session.tabGroups)
        tabGroups.append(place);

    settings.beginGroup(group);
    settings.setValue(QStringLiteral("urls"), session.urls);
    settings.setValue(QStringLiteral("histories"), histories);
    settings.setValue(QStringLiteral("groups"),
                      QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("groups"), groupList},
                                                                  {QStringLiteral("tabs"), tabGroups}})
                                            .toJson(QJsonDocument::Compact)));
    settings.setValue(QStringLiteral("currentTab"), session.currentTab);
    settings.endGroup();
}

Saved read(QSettings &settings, int maximumTabs, const QString &group)
{
    Saved session;
    settings.beginGroup(group);
    session.urls = settings.value(QStringLiteral("urls")).toStringList().mid(0, maximumTabs);
    const QStringList histories = settings.value(QStringLiteral("histories")).toStringList();
    const QJsonObject groups =
        QJsonDocument::fromJson(settings.value(QStringLiteral("groups")).toString().toUtf8()).object();
    session.currentTab = qBound(0, settings.value(QStringLiteral("currentTab")).toInt(),
                                qMax(0, int(session.urls.size()) - 1));
    settings.endGroup();

    for (const QJsonValue &value : groups.value(QStringLiteral("groups")).toArray()) {
        const QJsonObject entry = value.toObject();
        session.groups.append({entry.value(QStringLiteral("title")).toString(),
                               entry.value(QStringLiteral("color")).toInt(),
                               entry.value(QStringLiteral("collapsed")).toBool()});
    }
    const QJsonArray tabGroups = groups.value(QStringLiteral("tabs")).toArray();
    for (int index = 0; index < session.urls.size(); ++index) {
        session.histories.append(QByteArray::fromBase64(histories.value(index).toLatin1()));
        const int place = tabGroups.at(index).toInt(-1);
        session.tabGroups.append(place >= 0 && place < session.groups.size() ? place : -1);
    }
    return session;
}

bool markRunning(QSettings &settings)
{
    // Set while SpacePenguin runs and cleared when it quits; found set at start,
    // the last run ended without quitting.
    const bool unclean = settings.value(kRunning, false).toBool();
    settings.setValue(kRunning, true);
    return unclean;
}

void markClosed(QSettings &settings)
{
    settings.setValue(kRunning, false);
}

bool setAsideForRestore(QSettings &settings)
{
    if (settings.value(kGroup + QStringLiteral("/urls")).toStringList().isEmpty())
        return false;
    settings.remove(kCrashedGroup);
    for (const QString &key : {QStringLiteral("urls"), QStringLiteral("histories"), QStringLiteral("currentTab"),
                               QStringLiteral("groups")})
        settings.setValue(kCrashedGroup + QLatin1Char('/') + key, settings.value(kGroup + QLatin1Char('/') + key));
    return true;
}

}

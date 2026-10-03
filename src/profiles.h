#pragma once

#include <QString>

class QWebEngineProfile;

namespace spacepenguin {

QString defaultDataDirectory();

QWebEngineProfile *createProfile(QObject *parent, bool isPrivate);

void installStartPageHandler(QWebEngineProfile *profile);

}
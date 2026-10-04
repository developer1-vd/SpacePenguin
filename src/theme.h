#pragma once

#include <QList>
#include <QString>

namespace spacepenguin {

class Theme
{
public:
    enum class Mode { System, Light, Dark };

    static QList<Mode> modes();
    static QString modeKey(Mode mode);
    static QString modeLabel(Mode mode);
    static Mode modeFromKey(const QString &key);

    static Mode storedMode();
    static void storeMode(Mode mode);

    static void apply(Mode mode);
    static Mode effectiveMode();
};

}
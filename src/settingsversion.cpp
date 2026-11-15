#include "settingsversion.h"

#include <QSettings>

namespace spacepenguin::SettingsVersion {

QList<Step> steps()
{
    return {
        // 0 -> 1: versioning begins (1.0). Nothing moves; the profile is stamped.
        [](QSettings &) {},
    };
}

Outcome upgrade(QSettings &settings, const QList<Step> &steps, int current)
{
    Q_ASSERT(steps.size() == current);
    bool ok = false;
    int version = settings.value(QLatin1String(kKey)).toInt(&ok);
    if (!ok || version < 0)
        version = 0;
    if (version > current)
        return Outcome::Newer;
    if (version == current)
        return Outcome::UpToDate;
    for (; version < current && version < steps.size(); ++version) {
        steps.at(version)(settings);
        // After each step, so a crash part-way resumes at the next one.
        settings.setValue(QLatin1String(kKey), version + 1);
        settings.sync();
    }
    return Outcome::Upgraded;
}

}

#pragma once

#include <functional>

#include <QList>

class QSettings;

namespace spacepenguin {

/**
 * The version of the settings' layout, kept in the settings themselves so a
 * release can change how something is stored without breaking the profiles
 * people already have.
 *
 * To change the layout: write a step that moves the old keys to the new form,
 * append it to steps() in settingsversion.cpp and raise kCurrent by one. At
 * start-up every profile runs the steps it has not had yet, in order.
 */
namespace SettingsVersion {

/** The layout this build writes. Profiles from before versioning count as 0. */
inline constexpr int kCurrent = 1;

/** Where the number is kept. */
inline constexpr char kKey[] = "settingsVersion";

/** One step: brings settings at version N to version N + 1. */
using Step = std::function<void(QSettings &)>;

/** The steps, the first taking version 0 to 1. Exactly kCurrent of them. */
QList<Step> steps();

/** What upgrade() did. */
enum class Outcome {
    UpToDate, // nothing to do
    Upgraded, // ran one or more steps
    Newer,    // written by a later release: left as it is, not downgraded
};

/**
 * Runs the steps @p settings has not had and records the new version. Settings
 * from a newer release are left untouched, so going back to an older build
 * never damages them.
 */
Outcome upgrade(QSettings &settings, const QList<Step> &steps = SettingsVersion::steps(),
                int current = kCurrent);

}

}
